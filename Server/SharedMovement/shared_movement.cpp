#include <algorithm>
#include <utility>
#include "shared_movement.h"

constexpr float DEGREES_TO_RADIANS = 3.14159265358979f / 180.0f;

float GetMaxMoveSpeed(std::int8_t input_forward, bool is_aiming)
{
	if (is_aiming) return MOVE_SPEED_AIMING;
	return input_forward < 0 ? MOVE_SPEED_BACKWARD : MOVE_SPEED_FORWARD;
}

MoveVector GetMoveInputDirection(float yaw, std::int8_t input_right, std::int8_t input_forward)
{
	const float radians = yaw * DEGREES_TO_RADIANS;
	const float forward = input_forward / 127.0f;
	const float right = input_right / 127.0f;
	MoveVector direction{ forward * std::cos(radians) - right * std::sin(radians), forward * std::sin(radians) + right * std::cos(radians), 0.0f };
	// 입력 세기는 1을 넘지 않는다.
	const float length = Length(direction);
	if (length > 1.0f) direction = direction * (1.0f / length);
	return direction;
}

// 제동만 받는 속력이 seconds초 뒤 되는 값과 그동안 간 거리. 마찰은 속력에 비례하고 제동 감속은 일정하다.
// 속력 s(t) = (s0 + k)·e^(−f·t) − k (k = 감속 / 마찰)이고, floor 아래로는 내려가지 않는다.
static std::pair<float, float> BrakeSpeed(float speed, float friction, float deceleration, float floor, float seconds)
{
	if (speed <= floor) return { speed, speed * seconds };
	if (friction <= 0.0f) {
		if (deceleration <= 0.0f) return { speed, speed * seconds };
		const float stop_time = std::min(seconds, (speed - floor) / deceleration);
		const float end_speed = speed - deceleration * stop_time;
		return { end_speed, (speed + end_speed) * 0.5f * stop_time + end_speed * (seconds - stop_time) };
	}
	const float k = deceleration / friction;
	// floor에 닿는 시간
	const float reach_time = std::log((speed + k) / (floor + k)) / friction;
	const float time = std::min(seconds, reach_time);
	const float end_speed = std::max(floor, (speed + k) * std::exp(-friction * time) - k);
	const float distance = (speed + k) * (1.0f - std::exp(-friction * time)) / friction - k * time;
	return { end_speed, distance + end_speed * (seconds - time) };
}

MoveStep CalcWalkStep(const MoveVector& velocity, const MoveVector& input, const MoveWalkParams& params, float seconds)
{
	const MoveVector old_velocity = Horizontal(velocity);
	if (seconds < MOVE_MIN_TICK_TIME) return { old_velocity, {} };
	const float input_size = Length(input);
	const bool has_input = input_size > 0.0f;
	// 입력 세기만큼 최고 속도도 줄어든다.
	const float max_input_speed = params.max_speed * std::min(1.0f, input_size);
	const float speed = Length(old_velocity);
	const bool is_over_max = speed * speed > max_input_speed * max_input_speed * MOVE_OVER_SPEED_RATIO;

	MoveVector next = old_velocity;
	MoveVector delta;
	if (!has_input || is_over_max) {
		// 마찰(× 제동 마찰 배수)과 제동 감속으로 줄인다. 방향은 그대로다. 입력이 지금 속도 쪽이면 최고 속도 아래로는 줄이지 않는다.
		const float friction = params.ground_friction * MOVE_BRAKING_FRICTION_FACTOR;
		const float floor_speed = has_input && Dot(input, old_velocity) > 0.0f ? max_input_speed : 0.0f;
		const auto [braked, distance] = BrakeSpeed(speed, friction, params.braking_deceleration, floor_speed, seconds);
		const MoveVector direction = SafeNormal(old_velocity);
		next = direction * braked;
		delta = direction * distance;
		// 제동 감속이 있으면 아주 느려졌을 때 멈춘다.
		if (braked * braked <= MOVE_SMALL_NUMBER || (params.braking_deceleration > 0.0f && braked <= MOVE_BRAKE_TO_STOP_SPEED)) next = {};
		if (has_input) {
			// 상한을 넘은 채 입력이 있으면 가속은 방향만 바꾸고 지금 속력을 넘지 않는다.
			const MoveVector braked_velocity = next;
			next = next + input * (MOVE_MAX_ACCELERATION * seconds);
			const float limit = Length(braked_velocity);
			const float next_speed = Length(next);
			if (next_speed > limit && next_speed > 0.0f) next = next * (limit / next_speed);
			delta = delta + (next - braked_velocity) * (0.5f * seconds);
		}
		return { next, delta };
	}

	// 속도 방향을 입력 쪽으로 돌린다. 지난 시간이 길수록 1 − e^(−마찰·시간)만큼 입력 방향에 가까워진다.
	const MoveVector input_direction = input * (1.0f / input_size);
	const float turn = 1.0f - std::exp(-params.ground_friction * seconds);
	next = next - (next - input_direction * speed) * turn;
	// 입력 방향으로 가속하고 최고 속도로 자른다. 돌린 뒤 이미 상한을 넘었으면 그 속력까지만 둔다.
	const float turned_speed = Length(next);
	const float limit = turned_speed * turned_speed > max_input_speed * max_input_speed * MOVE_OVER_SPEED_RATIO ? turned_speed : max_input_speed;
	const MoveVector accelerated = next + input * (MOVE_MAX_ACCELERATION * seconds);
	const float accelerated_speed = Length(accelerated);
	next = accelerated_speed > limit && accelerated_speed > 0.0f ? accelerated * (limit / accelerated_speed) : accelerated;
	// 간 거리: 속력은 일정한 가속으로 늘다가 상한에서 멈추고, 방향은 처음과 끝 속도의 가운데로 본다.
	const float acceleration = MOVE_MAX_ACCELERATION * std::min(1.0f, input_size);
	const float end_speed = Length(next);
	float distance;
	if (speed + acceleration * seconds <= end_speed + MOVE_SMALL_NUMBER || acceleration <= 0.0f) {
		distance = (speed + end_speed) * 0.5f * seconds;
	}
	else {
		const float reach_time = std::clamp((end_speed - speed) / acceleration, 0.0f, seconds);
		distance = (speed + end_speed) * 0.5f * reach_time + end_speed * (seconds - reach_time);
	}
	MoveVector direction = SafeNormal(old_velocity + next);
	if (IsNearlyZero(direction)) direction = SafeNormal(next);
	return { next, direction * distance };
}

MoveStep CalcFallStep(const MoveVector& velocity, float seconds)
{
	if (seconds <= 0.0f) return { velocity, {} };
	MoveVector next = velocity;
	MoveVector delta{ velocity.x * seconds, velocity.y * seconds, 0.0f };
	// 중력으로 빨라지다가 낙하 속도 상한에 닿으면 그 뒤는 일정한 속도로 떨어진다.
	const float falling = velocity.z - GRAVITY * seconds;
	if (falling >= -TERMINAL_FALL_SPEED || velocity.z < -TERMINAL_FALL_SPEED) {
		next.z = velocity.z < -TERMINAL_FALL_SPEED ? -TERMINAL_FALL_SPEED : falling;
		delta.z = (velocity.z + next.z) * 0.5f * seconds;
	}
	else {
		const float reach_time = (velocity.z + TERMINAL_FALL_SPEED) / GRAVITY;
		next.z = -TERMINAL_FALL_SPEED;
		delta.z = (velocity.z + next.z) * 0.5f * reach_time + next.z * (seconds - reach_time);
	}
	return { next, delta };
}

float GetSimulationTimeStep(float remaining, int iterations)
{
	if (remaining > MOVE_MAX_SIMULATION_TIME_STEP && iterations < MOVE_MAX_SIMULATION_ITERATIONS) {
		remaining = std::min(MOVE_MAX_SIMULATION_TIME_STEP, remaining * 0.5f);
	}
	return std::max(MOVE_MIN_TICK_TIME, remaining);
}

float PullBackHitTime(float time, float delta_length)
{
	if (delta_length <= 0.0f) return std::clamp(time, 0.0f, 1.0f);
	// 되돌리는 거리는 이동 길이의 일정 비율이되 최소·최대 거리 사이로 두고, 이동의 일정 비율을 더 되돌린다.
	const float back = std::clamp(MOVE_HIT_PULLBACK_RATIO, MOVE_HIT_PULLBACK_MIN_DISTANCE / delta_length, MOVE_HIT_PULLBACK_MAX_DISTANCE / delta_length) + MOVE_HIT_PULLBACK_EXTRA_RATIO;
	return std::clamp(time - back, 0.0f, 1.0f);
}

bool IsMovingOutOfPenetration(const MoveHit& hit, const MoveVector& delta)
{
	return hit.is_start_penetrating && Dot(hit.impact_normal, SafeNormal(delta)) > 0.0f;
}

MoveVector GetPenetrationAdjustment(const MoveHit& hit)
{
	if (!hit.is_start_penetrating) return {};
	const float depth = hit.penetration_depth > 0.0f ? hit.penetration_depth : MOVE_PENETRATION_PULLBACK_DISTANCE;
	return hit.normal * (depth + MOVE_PENETRATION_PULLBACK_DISTANCE);
}

bool IsWalkable(const MoveHit& hit)
{
	if (!hit.is_blocking || hit.is_start_penetrating) return false;
	return hit.impact_normal.z >= MOVE_SMALL_NUMBER && hit.impact_normal.z >= MOVE_WALKABLE_FLOOR_Z;
}

bool IsWithinEdgeTolerance(const MoveVector& capsule_location, const MoveVector& impact_point, float radius)
{
	const float reduced = std::max(MOVE_SWEEP_EDGE_REJECT_DISTANCE + MOVE_SMALL_NUMBER, radius - MOVE_SWEEP_EDGE_REJECT_DISTANCE);
	return LengthSquared(Horizontal(impact_point - capsule_location)) < reduced * reduced;
}

MoveVector ComputeGroundMovementDelta(const MoveVector& delta, const MoveHit& ramp_hit, bool is_line_trace)
{
	const MoveVector& floor_normal = ramp_hit.impact_normal;
	if (floor_normal.z < 1.0f - MOVE_SMALL_NUMBER && floor_normal.z > MOVE_SMALL_NUMBER && ramp_hit.normal.z > MOVE_SMALL_NUMBER && !is_line_trace && IsWalkable(ramp_hit)) {
		// 수평 이동은 그대로 두고 비탈면에 붙도록 높이만 바꾼다.
		MoveVector ramp = delta;
		ramp.z = -Dot(floor_normal, delta) / floor_normal.z;
		return ramp;
	}
	return delta;
}

MoveVector ComputeSlideVector(const MoveVector& delta, float time, const MoveVector& normal)
{
	return (delta - normal * Dot(delta, normal)) * time;
}

MoveVector HandleSlopeBoosting(const MoveVector& slide, const MoveVector& delta, float time, const MoveVector& normal)
{
	MoveVector result = slide;
	if (result.z <= 0.0f) return result;
	// 원래 가려던 높이보다 높이 올라가지 않는다.
	const float z_limit = delta.z * time;
	if (result.z - z_limit <= MOVE_SMALL_NUMBER) return result;
	if (z_limit > 0.0f) {
		// 방향은 그대로 두고 전체를 줄여야 다시 면으로 파고들지 않는다.
		result = result * (z_limit / result.z);
	}
	else {
		// 내려가던 중 위로 꺾이면 꺾인 만큼은 없앤다.
		result = {};
	}
	// 남은 몫은 수평으로, 면에 나란하게 간다.
	const MoveVector remainder = Horizontal(slide - result);
	const MoveVector normal_horizontal = SafeNormal(Horizontal(normal));
	return result + ComputeSlideVector(remainder, 1.0f, normal_horizontal);
}

MoveVector GetWalkingSlideNormal(const MoveVector& normal, const MoveHit& hit, const MoveVector& delta, const MoveFloor& floor)
{
	MoveVector result = normal;
	if (result.z > 0.0f) {
		// 걸을 수 없는 비탈에 밀려 올라가지 않는다.
		if (!IsWalkable(hit)) result = SafeNormal(Horizontal(result));
	}
	else if (result.z < -MOVE_SMALL_NUMBER) {
		// 캡슐 윗부분이 닿아도 바닥으로 밀려 내려가지 않는다.
		if (floor.floor_distance < MOVE_MIN_FLOOR_DISTANCE && floor.is_blocking) {
			const MoveVector& floor_normal = floor.hit.normal;
			if (Dot(delta, floor_normal) < 0.0f && floor_normal.z < 1.0f - 1e-5f) result = floor_normal;
			result = SafeNormal(Horizontal(result));
		}
	}
	return result;
}

MoveVector TwoWallAdjust(const MoveVector& delta, const MoveHit& hit, const MoveVector& old_hit_normal, bool is_walking, const MoveFloor& floor)
{
	MoveVector result = delta;
	const MoveVector& hit_normal = hit.normal;
	if (Dot(old_hit_normal, hit_normal) <= 0.0f) {
		// 90도 이하 구석이면 두 면이 만나는 선을 따라 간다.
		const MoveVector direction = SafeNormal(Cross(hit_normal, old_hit_normal));
		result = direction * (Dot(delta, direction) * (1.0f - hit.time));
		if (Dot(delta, result) < 0.0f) result = -result;
	}
	else {
		// 새 면을 따라 간다. 공중이면 원래보다 높이 올라가지 않게 한다.
		result = ComputeSlideVector(delta, 1.0f - hit.time, hit_normal);
		if (!is_walking) result = HandleSlopeBoosting(result, delta, 1.0f - hit.time, hit_normal);
		if (Dot(result, delta) <= 0.0f) {
			result = {};
		}
		else if (std::fabs(Dot(hit_normal, old_hit_normal) - 1.0f) < MOVE_SMALL_NUMBER) {
			// 같은 벽에 또 닿으면 반올림 오차일 수 있으니 조금 떼어 낸다.
			result = result + hit_normal * MOVE_SAME_WALL_NUDGE_DISTANCE;
		}
	}
	if (!is_walking) return result;
	if (result.z > 0.0f) {
		// 걸을 수 있는 비탈은 수평 속도를 그대로 둔 채 오르고, 걸을 수 없는 면은 세운 벽처럼 수평으로만 간다.
		if ((hit_normal.z >= MOVE_WALKABLE_FLOOR_Z || IsWalkable(hit)) && hit_normal.z > MOVE_SMALL_NUMBER) {
			const float time = 1.0f - hit.time;
			const MoveVector scaled = SafeNormal(result) * Length(delta);
			result = (Horizontal(delta) + MoveVector{ 0.0f, 0.0f, scaled.z / hit_normal.z }) * time;
			// 턱 높이보다 높이 오르지 않는다.
			if (result.z > MOVE_MAX_STEP_HEIGHT) result = result * (MOVE_MAX_STEP_HEIGHT / result.z);
		}
		else {
			result = Horizontal(result);
		}
	}
	else if (result.z < 0.0f) {
		// 바닥으로 파고들지 않는다.
		if (floor.floor_distance < MOVE_MIN_FLOOR_DISTANCE && floor.is_blocking) result = Horizontal(result);
	}
	return result;
}

float GetFloorHeightAdjustment(const MoveFloor& floor)
{
	if (!floor.is_walkable) return 0.0f;
	float distance = floor.floor_distance;
	if (floor.is_line_trace) {
		// 줄 검사로 찾은 바닥으로 걸을 수 없는 벽을 타고 오르지 않는다.
		if (distance < MOVE_MIN_FLOOR_DISTANCE && floor.line_distance >= MOVE_MIN_FLOOR_DISTANCE) return 0.0f;
		distance = floor.line_distance;
	}
	if (distance >= MOVE_MIN_FLOOR_DISTANCE && distance <= MOVE_MAX_FLOOR_DISTANCE) return 0.0f;
	return (MOVE_MIN_FLOOR_DISTANCE + MOVE_MAX_FLOOR_DISTANCE) * 0.5f - distance;
}

float GetSweepFloorDistance(float hit_time, float trace_distance, float shrink_height)
{
	const float max_penetration = std::max(MOVE_MAX_FLOOR_DISTANCE, CHARACTER_CAPSULE_RADIUS);
	return std::max(-max_penetration, hit_time * trace_distance - shrink_height);
}

float GetFloorSweepDistance(bool is_walking)
{
	const float height_adjust = is_walking ? MOVE_MAX_FLOOR_DISTANCE + MOVE_SMALL_NUMBER : -MOVE_MAX_FLOOR_DISTANCE;
	return std::max(MOVE_MAX_FLOOR_DISTANCE, MOVE_MAX_STEP_HEIGHT + height_adjust);
}

MoveStepUpStart GetStepUpStart(const MoveVector& location, const MoveHit& hit, const MoveFloor& floor, bool is_walking)
{
	MoveStepUpStart start;
	if (!hit.is_blocking || hit.is_start_penetrating || !is_walking) return start;
	// 캡슐 위쪽이 닿았으면 오르지 않는다.
	const float impact_z = hit.impact_point.z;
	if (impact_z > location.z + (CHARACTER_CAPSULE_HALF_HEIGHT - CHARACTER_CAPSULE_RADIUS)) return start;
	start.up_height = MOVE_MAX_STEP_HEIGHT;
	start.down_height = MOVE_MAX_STEP_HEIGHT;
	float floor_base_z = location.z - CHARACTER_CAPSULE_HALF_HEIGHT;
	start.floor_point_z = floor_base_z;
	if (floor.is_walkable) {
		// 바닥에서 띄운 만큼은 턱 높이에서 빼고, 실제 바닥 점 기준으로 턱 높이를 잰다.
		const float floor_distance = std::max(0.0f, floor.is_line_trace ? floor.line_distance : floor.floor_distance);
		floor_base_z -= floor_distance;
		start.up_height = std::max(MOVE_MAX_STEP_HEIGHT - floor_distance, 0.0f);
		start.down_height = MOVE_MAX_STEP_HEIGHT + MOVE_MAX_FLOOR_DISTANCE * 2.0f;
		const bool is_vertical_face = !IsWithinEdgeTolerance(hit.location, hit.impact_point);
		if (!floor.is_line_trace && !is_vertical_face) start.floor_point_z = floor.hit.impact_point.z;
		else start.floor_point_z -= floor.floor_distance;
	}
	// 닿은 점이 발보다 낮으면 오를 것이 없다.
	if (impact_z <= floor_base_z) return start;
	start.can_step_up = true;
	return start;
}

bool IsStepDownAcceptable(const MoveHit& down_hit, const MoveVector& delta, const MoveVector& old_location, float floor_point_z)
{
	if (down_hit.impact_point.z - floor_point_z > MOVE_MAX_STEP_HEIGHT) return false;
	if (!IsWalkable(down_hit)) {
		// 걸을 수 없는 면이 앞을 막거나, 그 면 위로 처음보다 높아지면 받아들이지 않는다. 아래로 내려가며 디디는 것은 괜찮다.
		if (Dot(delta, down_hit.impact_normal) < 0.0f) return false;
		if (down_hit.location.z > old_location.z) return false;
	}
	return IsWithinEdgeTolerance(down_hit.location, down_hit.impact_point);
}

bool IsLandingHitAcceptable(const MoveHit& hit)
{
	if (!hit.is_blocking) return false;
	if (hit.is_start_penetrating) return hit.normal.z >= MOVE_SMALL_NUMBER;
	if (!IsWalkable(hit)) return false;
	// 아래 반구에 닿아야 한다.
	const float lower_hemisphere_z = hit.location.z - CHARACTER_CAPSULE_HALF_HEIGHT + CHARACTER_CAPSULE_RADIUS;
	if (hit.impact_point.z >= lower_hemisphere_z) return false;
	return IsWithinEdgeTolerance(hit.location, hit.impact_point);
}

bool IsSameNormal(const MoveVector& a, const MoveVector& b)
{
	return std::fabs(a.x - b.x) <= MOVE_SMALL_NUMBER && std::fabs(a.y - b.y) <= MOVE_SMALL_NUMBER && std::fabs(a.z - b.z) <= MOVE_SMALL_NUMBER;
}

bool ShouldCheckForValidLandingSpot(const MoveVector& location, const MoveHit& hit)
{
	return hit.normal.z > MOVE_SMALL_NUMBER && !IsSameNormal(hit.normal, hit.impact_normal) && IsWithinEdgeTolerance(location, hit.impact_point);
}

MoveVector GetLedgeNormal(const MoveFloor& ledge_floor)
{
	if (!ledge_floor.is_blocking || ledge_floor.hit.normal.z <= MOVE_SMALL_NUMBER) return {};
	// 바닥 면 법선과 접촉 법선이 이루는 면에서 면 법선에 수직인 수평 방향이 가장자리 바깥쪽이다.
	const MoveVector& impact_normal = ledge_floor.hit.impact_normal;
	const MoveVector ledge_normal = Cross(impact_normal, Cross(impact_normal, ledge_floor.hit.normal));
	return SafeNormal(Horizontal(ledge_normal));
}

MoveVector GetLedgeSideMove(const MoveVector& delta, bool is_left)
{
	const MoveVector side{ delta.y, -delta.x, 0.0f };
	return is_left ? side : -side;
}
