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
// 벽에 막혔을 때 이동 방향을 돌려 다시 해 보는 각도(도). 돌린 방향으로는 원래 이동을 그 방향에 투영한 거리만큼 간다.
constexpr float SLIDE_ANGLES[] = { 30.0f, 60.0f };

// 캡슐 중심 (x, y, z)가 이동을 막는 배치물과 겹치는지. 이동 검증과 같이 MOVE_TOLERANCE만큼 줄인 캡슐로 본다.
static bool IsBlocked(const MapData& map, float x, float y, float z)
{
	return map.GetCollisionObjects().IsCapsuleOverlappingObjects({ x, y, z }, CHARACTER_CAPSULE_RADIUS - MOVE_TOLERANCE, CHARACTER_CAPSULE_HALF_HEIGHT - MOVE_TOLERANCE, TERRAIN_BLOCKS_PAWN);
}

// (x, y)에서 캡슐 중심 높이 low~high 사이에 설 수 있는 가장 낮은 높이. 지형에 선 높이와 배치물 윗면에 선 높이 중 높은 쪽이다.
// high에서도 막히면(벽이거나 오를 수 없는 높이) is_blocked이고, 그 범위에 바닥이 없으면 높이가 없다.
struct FloorSearch {
	bool is_blocked;
	std::optional<float> z;
};

static FloorSearch FindFloor(const MapData& map, float x, float y, float low, float high)
{
	const std::optional<float> ground = map.GetLandscape().GetGroundHeight(x, y);
	const float land = ground ? *ground + CHARACTER_CAPSULE_HALF_HEIGHT : -std::numeric_limits<float>::infinity();
	if (land > high || IsBlocked(map, x, y, high)) return { true, std::nullopt };
	const float bottom = std::max(low, land);
	if (!IsBlocked(map, x, y, bottom)) return { false, land >= low ? std::optional<float>(land) : std::nullopt };
	// bottom은 배치물과 겹치고 high는 겹치지 않으므로 그 사이 경계가 배치물 윗면이다. 줄인 캡슐의 경계라 줄인 만큼 올려 선다.
	float blocked = bottom;
	float free = high;
	for (int step = 0; step < FLOOR_SEARCH_STEPS; ++step) {
		const float middle = (blocked + free) * 0.5f;
		(IsBlocked(map, x, y, middle) ? blocked : free) = middle;
	}
	return { false, std::min(high, free + MOVE_TOLERANCE) };
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

// 수평 이동 (move_x, move_y)를 해 보고, 막히면 방향을 SLIDE_ANGLES만큼 돌려 벽을 따라 간다. 갈 수 있으면 간 거리와 그곳의 바닥 탐색 결과를 돌려준다.
struct HorizontalMove {
	float move_x;
	float move_y;
	FloorSearch floor;
};

static std::optional<HorizontalMove> TryHorizontalMove(const MapData& map, const MOVE_STATE& state, float move_x, float move_y, float low, float high)
{
	const auto attempt = [&](float x, float y) -> std::optional<HorizontalMove>
	{
		const FloorSearch floor = FindFloor(map, state.location[0] + x, state.location[1] + y, low, high);
		if (floor.is_blocked) return std::nullopt;
		return HorizontalMove{ x, y, floor };
	};
	if (std::optional<HorizontalMove> move = attempt(move_x, move_y)) return move;
	for (const float angle : SLIDE_ANGLES) {
		const float radians = DirectX::XMConvertToRadians(angle);
		const float cos_angle = std::cos(radians);
		const float sin_angle = std::sin(radians);
		for (const float side : { 1.0f, -1.0f }) {
			const float x = (move_x * cos_angle - side * move_y * sin_angle) * cos_angle;
			const float y = (side * move_x * sin_angle + move_y * cos_angle) * cos_angle;
			if (std::optional<HorizontalMove> move = attempt(x, y)) return move;
		}
	}
	return std::nullopt;
}

MOVE_STATE PredictMove(const MapData& map, const MOVE_STATE& state, std::int8_t input_right, std::int8_t input_forward, bool is_aiming, float seconds)
{
	MOVE_STATE next = state;
	for (float remaining = seconds; remaining > 0.0f; remaining -= MOVE_BRAKING_STEP_SECONDS) {
		const float step = std::min(remaining, MOVE_BRAKING_STEP_SECONDS);
		float* location = next.location;
		float* velocity = next.velocity;
		if (next.air_phase == AirPhase::GROUND) {
			const std::pair<float, float> walk_velocity = GetWalkVelocity(next, input_right, input_forward, is_aiming, step);
			velocity[0] = walk_velocity.first;
			velocity[1] = walk_velocity.second;
			const std::optional<HorizontalMove> move = TryHorizontalMove(map, next, velocity[0] * step, velocity[1] * step, location[2] - MOVE_MAX_STEP_HEIGHT, location[2] + MOVE_MAX_STEP_HEIGHT);
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

		// 공중: 수평 속도는 그대로이고 중력으로 수직 속도만 바뀐다. 수평으로 막히면 벽을 따라 가거나 멈춘다.
		const float vertical = GetFreeFallDisplacement(velocity[2], step);
		velocity[2] = std::max(-TERMINAL_FALL_SPEED, velocity[2] - GRAVITY * step);
		if (velocity[0] != 0.0f || velocity[1] != 0.0f) {
			const std::optional<HorizontalMove> move = TryHorizontalMove(map, next, velocity[0] * step, velocity[1] * step, location[2], location[2]);
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
			if (IsBlocked(map, location[0], location[1], target_z)) velocity[2] = 0.0f;
			else location[2] = target_z;
			if (velocity[2] <= 0.0f) next.air_phase = AirPhase::FALLING;
			continue;
		}
		// 내려가는 길에 바닥이 있으면 그 위에 선다.
		const FloorSearch floor = FindFloor(map, location[0], location[1], target_z, location[2]);
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
