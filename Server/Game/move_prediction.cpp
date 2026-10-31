#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>
#include <DirectXMath.h>
#include "move_prediction.h"
#include "MapData.h"
#include "air_state.h"

// 바닥 높이를 찾을 때 겹침 경계를 반으로 나눠 좁히는 횟수. 오르내릴 수 있는 높이 범위(2 × MOVE_MAX_STEP_HEIGHT)를 1cm 안으로 좁힌다.
constexpr int FLOOR_SEARCH_STEPS = 8;
// 벽에 막혔을 때 벽을 따라 갈 방향을 찾는다. 이동 방향을 0~SLIDE_MAX_ANGLE도 사이로 돌려 보며, 갈 수 있는 가장 작은 각도를 SLIDE_SEARCH_STEPS번 반으로 나눠 좁힌다.
// 돌린 방향으로는 원래 이동을 그 방향에 투영한 거리만큼 가므로, 평평한 벽이면 언리얼처럼 이동을 벽면에 투영한 만큼 간다.
constexpr float SLIDE_MAX_ANGLE = 89.0f;
constexpr int SLIDE_SEARCH_STEPS = 6;
// 걸을 수 있는 가장 가파른 면의 법선 위쪽 성분
static const float WALKABLE_FLOOR_NORMAL_Z = std::cos(DirectX::XMConvertToRadians(WALKABLE_FLOOR_DEGREES));

// 캡슐 중심 (x, y, z)가 이동을 막는 배치물과 겹치는지. 반지름과 반 높이를 margin만큼 줄인 캡슐로 본다.
static bool IsBlocked(const MapData& map, float x, float y, float z, float margin)
{
	return map.GetCollisionObjects().IsCapsuleOverlappingObjects({ x, y, z }, CHARACTER_CAPSULE_RADIUS - margin, CHARACTER_CAPSULE_HALF_HEIGHT - margin, TERRAIN_BLOCKS_PAWN);
}

// (x, y) 자리 랜드스케이프 면. 캡슐 아래 반구가 면에 닿는 캡슐 중심 높이와 면의 위쪽 법선이다.
// 면은 그 자리 높이를 지나고 앞뒤·좌우로 SLOPE_SAMPLE_DISTANCE 떨어진 높이로 구한 기울기의 평면으로 보며, 떨어진 높이가 없으면 평평하다고 본다.
struct LandscapeSurface {
	float contact_z;
	DirectX::XMFLOAT3 normal;
};

// 그 자리 높이가 없으면(랜드스케이프 밖이거나 구멍) 값이 없다.
static std::optional<LandscapeSurface> GetLandscapeSurface(const LandscapeData& landscape, float x, float y)
{
	const std::optional<float> ground = landscape.GetGroundHeight(x, y);
	if (!ground) return std::nullopt;
	const std::optional<float> ground_x0 = landscape.GetGroundHeight(x - SLOPE_SAMPLE_DISTANCE, y);
	const std::optional<float> ground_x1 = landscape.GetGroundHeight(x + SLOPE_SAMPLE_DISTANCE, y);
	const std::optional<float> ground_y0 = landscape.GetGroundHeight(x, y - SLOPE_SAMPLE_DISTANCE);
	const std::optional<float> ground_y1 = landscape.GetGroundHeight(x, y + SLOPE_SAMPLE_DISTANCE);
	float slope_x = 0.0f;
	float slope_y = 0.0f;
	if (ground_x0 && ground_x1 && ground_y0 && ground_y1) {
		slope_x = (*ground_x1 - *ground_x0) / (2.0f * SLOPE_SAMPLE_DISTANCE);
		slope_y = (*ground_y1 - *ground_y0) / (2.0f * SLOPE_SAMPLE_DISTANCE);
	}
	const float normal_z = 1.0f / std::sqrt(1.0f + slope_x * slope_x + slope_y * slope_y);
	return LandscapeSurface{ *ground + CHARACTER_CAPSULE_HALF_HEIGHT - CHARACTER_CAPSULE_RADIUS + CHARACTER_CAPSULE_RADIUS / normal_z, { -slope_x * normal_z, -slope_y * normal_z, normal_z } };
}

// (x, y)에서 캡슐 중심 높이 low~high 사이에 설 수 있는 가장 낮은 높이. 지형에 선 높이와 배치물 윗면에 선 높이 중 높은 쪽이다. 배치물 막힘은 margin만큼 줄인 캡슐로 본다.
// 그 높이에서 배치물에 막히고 high까지 올려도 막히면(벽이거나 오를 수 없는 높이) is_blocked이고, 그 범위에 바닥이 없으면 높이가 없다.
// 막히지 않으면 high에서 막히는지는 보지 않는다. 언리얼처럼 오를 때만 위를 보므로, 기울어진 벽이나 천장 밑도 바닥을 따라 지나간다.
struct FloorSearch {
	bool is_blocked;
	std::optional<float> z;
};

static FloorSearch FindFloor(const MapData& map, float x, float y, float low, float high, float margin)
{
	// 랜드스케이프 막힘은 이동 검증과 같이 MOVE_TOLERANCE만큼 파고드는 것까지는 보지 않는다. 기울기 평면으로 구한 접촉 높이의 오차를 덮는다.
	const std::optional<LandscapeSurface> surface = GetLandscapeSurface(map.GetLandscape(), x, y);
	if (surface && surface->contact_z - MOVE_TOLERANCE > high) return { true, std::nullopt };
	// 걸을 수 있는 랜드스케이프에는 언리얼처럼 CHARACTER_FLOOR_DISTANCE만큼 띄워 서고, 선 높이가 high를 조금 넘으면 그 높이에 선다.
	// 걸을 수 없는 가파른 비탈은 바닥으로 보지 않는다. 언리얼 캐릭터는 그 위에 서지 못하고 미끄러진다.
	const float land = surface && surface->normal.z >= WALKABLE_FLOOR_NORMAL_Z ? surface->contact_z + CHARACTER_FLOOR_DISTANCE : -std::numeric_limits<float>::infinity();
	if (land >= high) {
		if (IsBlocked(map, x, y, land, margin)) return { true, std::nullopt };
		return { false, land };
	}
	const float bottom = std::max(low, land);
	if (!IsBlocked(map, x, y, bottom, margin)) return { false, land >= low ? std::optional<float>(land) : std::nullopt };
	if (IsBlocked(map, x, y, high, margin)) return { true, std::nullopt };
	// bottom은 배치물과 겹치고 high는 겹치지 않으므로 그 사이 경계가 배치물 윗면이다. 줄인 캡슐의 경계라 줄인 만큼 올리고 CHARACTER_FLOOR_DISTANCE만큼 띄워 선다.
	float blocked = bottom;
	float free = high;
	for (int step = 0; step < FLOOR_SEARCH_STEPS; ++step) {
		const float middle = (blocked + free) * 0.5f;
		(IsBlocked(map, x, y, middle, margin) ? blocked : free) = middle;
	}
	return { false, std::min(high, free + margin + CHARACTER_FLOOR_DISTANCE) };
}

// 땅에서 seconds초 동안 이동 키 입력으로 가속·마찰·제동한 뒤의 수평 속도(x, y). 언리얼 걷기 속도 계산과 같은 식이다.
static std::pair<float, float> GetWalkVelocity(const MOVE_STATE& state, std::int8_t input_right, std::int8_t input_forward, bool is_aiming, float seconds)
{
	const float yaw = DirectX::XMConvertToRadians(state.yaw);
	float input_x = (input_forward * std::cos(yaw) - input_right * std::sin(yaw)) / 127.0f;
	float input_y = (input_forward * std::sin(yaw) + input_right * std::cos(yaw)) / 127.0f;
	const float magnitude = std::min(1.0f, std::hypot(input_x, input_y));
	if (magnitude > 0.0f) {
		const float length = std::hypot(input_x, input_y);
		input_x *= magnitude / length;
		input_y *= magnitude / length;
	}
	const float max_speed = GetMaxMoveSpeed(input_forward, is_aiming) * magnitude;
	float velocity_x = state.velocity[0];
	float velocity_y = state.velocity[1];
	const float speed = std::hypot(velocity_x, velocity_y);
	if (magnitude <= 0.0f || speed > max_speed) {
		// 입력이 없거나 상한보다 빠르면 제동한다. 입력이 있으면 상한 아래로는 줄이지 않는다.
		const float braked = std::max(magnitude > 0.0f ? max_speed : 0.0f, speed - (MOVE_GROUND_FRICTION * MOVE_BRAKING_FRICTION_FACTOR * speed + MOVE_BRAKING_DECELERATION) * seconds);
		const float ratio = speed > 0.0f ? braked / speed : 0.0f;
		return { velocity_x * ratio, velocity_y * ratio };
	}
	// 입력 방향으로 마찰만큼 속도 방향을 돌린 뒤 가속하고 상한으로 자른다.
	const float friction = std::min(seconds * MOVE_GROUND_FRICTION, 1.0f);
	velocity_x -= (velocity_x - input_x / magnitude * speed) * friction;
	velocity_y -= (velocity_y - input_y / magnitude * speed) * friction;
	velocity_x += input_x * MOVE_MAX_ACCELERATION * seconds;
	velocity_y += input_y * MOVE_MAX_ACCELERATION * seconds;
	const float accelerated = std::hypot(velocity_x, velocity_y);
	if (accelerated > max_speed) return { velocity_x * max_speed / accelerated, velocity_y * max_speed / accelerated };
	return { velocity_x, velocity_y };
}

// 수평 이동 (move_x, move_y)를 해 보고, 막히면 방향을 돌려 벽을 따라 간다. 갈 수 있으면 간 거리와 그곳의 바닥 탐색 결과를 돌려준다.
struct HorizontalMove {
	float move_x;
	float move_y;
	FloorSearch floor;
};

static std::optional<HorizontalMove> TryHorizontalMove(const MapData& map, const MOVE_STATE& state, float move_x, float move_y, float low, float high, float margin)
{
	const auto attempt = [&](float x, float y) -> std::optional<HorizontalMove>
	{
		const FloorSearch floor = FindFloor(map, state.location[0] + x, state.location[1] + y, low, high, margin);
		if (floor.is_blocked) return std::nullopt;
		return HorizontalMove{ x, y, floor };
	};
	if (std::optional<HorizontalMove> move = attempt(move_x, move_y)) return move;
	const auto rotated = [&](float angle, float side)
	{
		const float radians = DirectX::XMConvertToRadians(angle);
		const float cos_angle = std::cos(radians);
		const float sin_angle = std::sin(radians);
		return attempt((move_x * cos_angle - side * move_y * sin_angle) * cos_angle, (side * move_x * sin_angle + move_y * cos_angle) * cos_angle);
	};
	// 좌우 중 SLIDE_MAX_ANGLE까지 돌려 갈 수 있는 쪽마다 막히는 각도와 갈 수 있는 각도 사이를 좁히고, 원래 방향에 더 가까운 쪽으로 간다.
	std::optional<HorizontalMove> best;
	float best_angle = SLIDE_MAX_ANGLE;
	for (const float side : { 1.0f, -1.0f }) {
		std::optional<HorizontalMove> open = rotated(SLIDE_MAX_ANGLE, side);
		if (!open) continue;
		float blocked_angle = 0.0f;
		float open_angle = SLIDE_MAX_ANGLE;
		for (int step = 0; step < SLIDE_SEARCH_STEPS; ++step) {
			const float middle = (blocked_angle + open_angle) * 0.5f;
			if (std::optional<HorizontalMove> move = rotated(middle, side)) {
				open = move;
				open_angle = middle;
			}
			else blocked_angle = middle;
		}
		if (!best || open_angle < best_angle) {
			best = open;
			best_angle = open_angle;
		}
	}
	return best;
}

MOVE_STATE PredictMove(const MapData& map, const MOVE_STATE& state, std::int8_t input_right, std::int8_t input_forward, bool is_aiming, float seconds)
{
	MOVE_STATE next = state;
	// 배치물 막힘은 MOVE_PREDICT_MARGIN만큼 줄인 캡슐로 보고, 시작 위치가 그 캡슐로 이미 겹치면 이동 검증과 같이 MOVE_TOLERANCE만큼 줄인 캡슐로 본다.
	const float margin = IsBlocked(map, state.location[0], state.location[1], state.location[2], MOVE_PREDICT_MARGIN) ? MOVE_TOLERANCE : MOVE_PREDICT_MARGIN;
	for (float remaining = seconds; remaining > 0.0f; remaining -= MOVE_BRAKING_STEP_SECONDS) {
		const float step = std::min(remaining, MOVE_BRAKING_STEP_SECONDS);
		float* location = next.location;
		float* velocity = next.velocity;
		if (next.air_phase == AirPhase::GROUND) {
			const std::pair<float, float> walk_velocity = GetWalkVelocity(next, input_right, input_forward, is_aiming, step);
			velocity[0] = walk_velocity.first;
			velocity[1] = walk_velocity.second;
			const std::optional<HorizontalMove> move = TryHorizontalMove(map, next, velocity[0] * step, velocity[1] * step, location[2] - MOVE_MAX_STEP_HEIGHT, location[2] + MOVE_MAX_STEP_HEIGHT, margin);
			if (!move) {
				velocity[0] = 0.0f;
				velocity[1] = 0.0f;
				continue;
			}
			// 실제로 간 거리로 속도를 다시 정한다. 오르내릴 수 있는 높이 안에 바닥이 없으면 떨어지기 시작한다.
			location[0] += move->move_x;
			location[1] += move->move_y;
			velocity[0] = move->move_x / step;
			velocity[1] = move->move_y / step;
			if (move->floor.z) {
				location[2] = *move->floor.z;
				continue;
			}
			next.air_phase = AirPhase::FALLING;
			velocity[2] = 0.0f;
			continue;
		}

		// 공중: 수평 속도는 그대로이고 중력으로 수직 속도만 바뀐다. 수평으로 막히면 벽을 따라 가거나 멈추고, 가파른 비탈에 닿으면 면을 따라 미끄러진다.
		const float vertical = GetFreeFallDisplacement(velocity[2], step);
		velocity[2] = std::max(-TERMINAL_FALL_SPEED, velocity[2] - GRAVITY * step);
		if (velocity[0] != 0.0f || velocity[1] != 0.0f) {
			const std::optional<HorizontalMove> move = TryHorizontalMove(map, next, velocity[0] * step, velocity[1] * step, location[2], location[2], margin);
			if (move) {
				location[0] += move->move_x;
				location[1] += move->move_y;
				velocity[0] = move->move_x / step;
				velocity[1] = move->move_y / step;
			}
			else {
				velocity[0] = 0.0f;
				velocity[1] = 0.0f;
			}
		}
		const float target_z = location[2] + vertical;
		if (vertical > 0.0f) {
			// 오르다 천장에 닿으면 그 자리에서 떨어지기 시작한다.
			if (IsBlocked(map, location[0], location[1], target_z, margin)) velocity[2] = 0.0f;
			else location[2] = target_z;
			if (velocity[2] <= 0.0f) next.air_phase = AirPhase::FALLING;
			continue;
		}
		// 걸을 수 없는 가파른 랜드스케이프 비탈에 닿으면 언리얼처럼 서지 않고 면을 따라 미끄러지며 떨어진다. 면 아래로는 내려가지 않고, 속도에서 면으로 파고드는 성분을 뺀다.
		if (const std::optional<LandscapeSurface> surface = GetLandscapeSurface(map.GetLandscape(), location[0], location[1]); surface && surface->normal.z < WALKABLE_FLOOR_NORMAL_Z && target_z <= surface->contact_z) {
			location[2] = surface->contact_z;
			const float into = velocity[0] * surface->normal.x + velocity[1] * surface->normal.y + velocity[2] * surface->normal.z;
			if (into < 0.0f) {
				velocity[0] -= into * surface->normal.x;
				velocity[1] -= into * surface->normal.y;
				velocity[2] -= into * surface->normal.z;
			}
			next.air_phase = AirPhase::FALLING;
			continue;
		}
		// 내려가는 길에 바닥이 있으면 그 위에 선다.
		const FloorSearch floor = FindFloor(map, location[0], location[1], target_z, location[2], margin);
		if (floor.z) {
			location[2] = *floor.z;
			velocity[2] = 0.0f;
			next.air_phase = AirPhase::GROUND;
			continue;
		}
		location[2] = target_z;
		next.air_phase = AirPhase::FALLING;
	}
	return next;
}
