#include <algorithm>
#include <cmath>
#include "MoveSimulation.h"
#include "MapData.h"
#include "movement_settings.h"

// 쓸기 결과로 만든 바닥. 처음부터 겹친 쓸기는 막힌 바닥으로 보지 않는다.
static MoveFloor MakeSweepFloor(const MoveHit& hit, float floor_distance, bool is_walkable)
{
	MoveFloor floor;
	floor.is_blocking = hit.is_blocking && !hit.is_start_penetrating;
	floor.is_walkable = is_walkable;
	floor.floor_distance = floor_distance;
	floor.hit = hit;
	return floor;
}

static MoveVector Down(float distance)
{
	return { 0.0f, 0.0f, -distance };
}

MoveSimulation::MoveSimulation(const MapData& map, const MOVE_STATE& state)
	: map_(&map), location_{ state.location[0], state.location[1], state.location[2] }, velocity_{ state.velocity[0], state.velocity[1], state.velocity[2] }, yaw_(state.yaw), view_pitch_(state.view_pitch), is_walking_(state.air_phase == AirPhase::GROUND)
{
	// 걷는 중이면 지금 자리 바닥에서 시작한다.
	if (is_walking_) floor_ = FindFloor(location_, nullptr);
}

void MoveSimulation::Simulate(std::int8_t input_right, std::int8_t input_forward, bool is_aiming, float seconds)
{
	if (!map_) return;
	input_ = GetMoveInputDirection(yaw_, input_right, input_forward);
	walk_params_.max_speed = GetMaxMoveSpeed(input_forward, is_aiming);
	jump_apex_attempts_ = 0;
	StartNewPhysics(seconds, 0);
}

void MoveSimulation::StopHorizontal()
{
	velocity_.x = 0.0f;
	velocity_.y = 0.0f;
}

void MoveSimulation::SetView(float yaw, float view_pitch)
{
	yaw_ = yaw;
	view_pitch_ = view_pitch;
}

MOVE_STATE MoveSimulation::GetMoveState() const
{
	MOVE_STATE state{};
	state.yaw = yaw_;
	state.view_pitch = view_pitch_;
	state.location[0] = location_.x;
	state.location[1] = location_.y;
	state.location[2] = location_.z;
	state.velocity[0] = velocity_.x;
	state.velocity[1] = velocity_.y;
	state.velocity[2] = velocity_.z;
	// 클라이언트와 같이 공중에서 올라가는 중이면 상승, 아니면 하강이다.
	state.air_phase = is_walking_ ? AirPhase::GROUND : velocity_.z > 0.0f ? AirPhase::RISING : AirPhase::FALLING;
	return state;
}

MoveHit MoveSimulation::SweepWorld(const MapData& map, const MoveVector& start, const MoveVector& end, float radius, float half_height, bool is_ignoring_moving_out)
{
	const MoveHit object_hit = map.GetCollisionObjects().SweepCapsule(start, end, radius, half_height, TERRAIN_BLOCKS_PAWN, is_ignoring_moving_out);
	const MoveHit landscape_hit = map.GetLandscape().SweepCapsule(start, end, radius, half_height, is_ignoring_moving_out);
	return GetFirstBlockingHit(object_hit, landscape_hit, end - start);
}

bool MoveSimulation::IsOverlappingWorld(const MapData& map, const MoveVector& center, float radius, float half_height)
{
	return map.GetCollisionObjects().IsCapsuleOverlappingObjects({ center.x, center.y, center.z }, radius, half_height, TERRAIN_BLOCKS_PAWN) || map.GetLandscape().IsCapsuleOverlapping(center, radius, half_height);
}

// 언리얼 바닥 찾기와 같은 순서다. 아래로 쓸 때는 바닥에 이미 닿아 있어도 이상한 결과가 나오지 않도록 캡슐 원기둥 부분을 줄인다.
// 쓸기가 캡슐 가장자리에 닿았거나 처음부터 겹쳤으면 반지름과 원기둥을 더 줄여 다시 쓸고, 줄인 높이만큼 바닥 거리에서 뺀다.
MoveFloor MoveSimulation::ComputeFloorDist(const MapData& map, const MoveVector& location, float line_distance, float sweep_distance, float sweep_radius, const MoveHit* downward_sweep)
{
	MoveFloor floor;
	bool is_skipping_sweep = false;
	if (downward_sweep && downward_sweep->is_blocking && !downward_sweep->is_start_penetrating) {
		// 방금 쓴 것이 아래로 곧게 쓴 것이면 그 결과를 바닥으로 쓴다.
		const MoveVector trace = downward_sweep->trace_start - downward_sweep->trace_end;
		if (trace.z > 0.0f && LengthSquared(Horizontal(trace)) <= MOVE_SMALL_NUMBER && IsWithinEdgeTolerance(downward_sweep->location, downward_sweep->impact_point)) {
			is_skipping_sweep = true;
			const bool is_walkable = IsWalkable(*downward_sweep);
			floor = MakeSweepFloor(*downward_sweep, location.z - downward_sweep->location.z, is_walkable);
			if (is_walkable) return floor;
		}
	}
	if (sweep_distance < line_distance) return floor;

	if (!is_skipping_sweep && sweep_distance > 0.0f && sweep_radius > 0.0f) {
		const float cylinder = CHARACTER_CAPSULE_HALF_HEIGHT - CHARACTER_CAPSULE_RADIUS;
		float shrink_height = cylinder * (1.0f - MOVE_FLOOR_SWEEP_SHRINK_SCALE);
		float trace_distance = sweep_distance + shrink_height;
		float radius = sweep_radius;
		float half_height = CHARACTER_CAPSULE_HALF_HEIGHT - shrink_height;
		MoveHit hit = SweepWorld(map, location, location + Down(trace_distance), radius, half_height, false);
		if (hit.is_blocking) {
			if (hit.is_start_penetrating || !IsWithinEdgeTolerance(location, hit.impact_point, radius)) {
				radius = std::max(0.0f, radius - MOVE_SWEEP_EDGE_REJECT_DISTANCE - MOVE_SMALL_NUMBER);
				shrink_height = cylinder * (1.0f - MOVE_FLOOR_SWEEP_RETRY_SHRINK_SCALE);
				trace_distance = sweep_distance + shrink_height;
				half_height = std::max(CHARACTER_CAPSULE_HALF_HEIGHT - shrink_height, radius);
				hit = SweepWorld(map, location, location + Down(trace_distance), radius, half_height, false);
			}
			// 바닥 거리는 음수일 수 있다. 바닥에 파고든 만큼 위로 빠져나오는 데 쓴다.
			const float sweep_result = GetSweepFloorDistance(hit.time, trace_distance, shrink_height);
			floor = MakeSweepFloor(hit, sweep_result, false);
			if (floor.is_blocking && IsWalkable(hit) && sweep_result <= sweep_distance) {
				floor.is_walkable = true;
				return floor;
			}
		}
	}

	// 쓸기가 아무것에도 닿지 않았으면 선분 검사도 하지 않는다. 처음부터 겹쳤으면 선분으로 다시 본다.
	if (!floor.is_blocking && !floor.hit.is_start_penetrating) {
		floor.floor_distance = sweep_distance;
		return floor;
	}
	if (line_distance > 0.0f) {
		// 캡슐 중심에서 아래로 본다. 캡슐 반 높이만큼 위에서 시작하므로 그만큼 뺀다.
		const float trace_distance = line_distance + CHARACTER_CAPSULE_HALF_HEIGHT;
		const MoveHit hit = SweepWorld(map, location, location + Down(trace_distance), 0.0f, 0.0f, false);
		if (hit.is_blocking && hit.time > 0.0f) {
			const float line_result = GetSweepFloorDistance(hit.time, trace_distance, CHARACTER_CAPSULE_HALF_HEIGHT);
			floor.is_blocking = true;
			if (line_result <= line_distance && IsWalkable(hit) && floor.hit.is_blocking) {
				// 면 법선은 선분 결과를 쓰고, 닿은 비율·위치는 캡슐 쓸기 결과를 그대로 둔다.
				MoveHit merged = hit;
				merged.time = floor.hit.time;
				merged.impact_point = floor.hit.impact_point;
				merged.location = floor.hit.location;
				merged.trace_start = floor.hit.trace_start;
				merged.trace_end = floor.hit.trace_end;
				floor.hit = merged;
				floor.is_line_trace = true;
				floor.line_distance = line_result;
				floor.is_walkable = true;
				return floor;
			}
		}
	}
	floor.is_walkable = false;
	return floor;
}

// 캐릭터 설정의 가장자리 걸치기 반경이 0이라 걸치기 검사는 하지 않는다.
MoveFloor MoveSimulation::FindFloor(const MoveVector& location, const MoveHit* downward_sweep) const
{
	const float distance = GetFloorSweepDistance(is_walking_);
	return ComputeFloorDist(*map_, location, distance, distance, CHARACTER_CAPSULE_RADIUS, downward_sweep);
}

// 캡슐을 delta만큼 쓸어 옮긴다. 막히면 닿은 자리에서 조금 되돌린 곳에 선다. 되돌린 결과 거의 움직이지 않으면 제자리에 둔다.
MoveHit MoveSimulation::Move(const MoveVector& delta, bool is_ignoring_moving_out)
{
	const MoveVector end = location_ + delta;
	if (LengthSquared(delta) <= MOVE_MIN_MOVE_DISTANCE * MOVE_MIN_MOVE_DISTANCE) return MoveHit{ false, false, 1.0f, location_, end };
	MoveHit hit = SweepWorld(*map_, location_, end, CHARACTER_CAPSULE_RADIUS, CHARACTER_CAPSULE_HALF_HEIGHT, is_ignoring_moving_out);
	if (!hit.is_blocking) {
		location_ = end;
		return hit;
	}
	hit.time = PullBackHitTime(hit.time, Length(delta));
	const MoveVector moved = delta * hit.time;
	if (LengthSquared(moved) <= MOVE_MIN_MOVE_DISTANCE * MOVE_MIN_MOVE_DISTANCE) hit.time = 0.0f;
	else location_ = location_ + moved;
	return hit;
}

// 처음부터 겹쳐 있으면 빠져나온 뒤 같은 이동을 다시 해 본다.
MoveHit MoveSimulation::SafeMove(const MoveVector& delta)
{
	MoveHit hit = Move(delta, false);
	if (hit.is_start_penetrating && ResolvePenetration(GetPenetrationAdjustment(hit), hit)) hit = Move(delta, false);
	return hit;
}

// 빠져나올 자리가 비었으면 바로 옮기고, 아니면 빠져나오는 쪽으로 쓸어 갈 수 있는 만큼 간다. 그래도 못 움직이면 두 겹침을 합친 방향, 원래 이동을 더한 방향, 원래 이동 순으로 해 본다.
bool MoveSimulation::ResolvePenetration(const MoveVector& adjustment, const MoveHit& hit)
{
	if (IsNearlyZero(adjustment, 0.0f)) return false;
	const float inflation = MOVE_PENETRATION_OVERLAP_INFLATION;
	if (!IsOverlappingWorld(*map_, hit.trace_start + adjustment, CHARACTER_CAPSULE_RADIUS + inflation, CHARACTER_CAPSULE_HALF_HEIGHT + inflation)) {
		location_ = location_ + adjustment;
		return true;
	}
	const auto move_out = [&](const MoveVector& delta)
	{
		const MoveVector before = location_;
		const MoveHit result = Move(delta, true);
		return std::make_pair(!IsNearlyZero(location_ - before, 0.0f), result);
	};
	auto [is_moved, sweep_out] = move_out(adjustment);
	if (!is_moved && sweep_out.is_start_penetrating) {
		const MoveVector second = GetPenetrationAdjustment(sweep_out);
		const MoveVector combined = adjustment + second;
		if (!IsNearlyZero(second - adjustment, 0.0f) && !IsNearlyZero(combined, 0.0f)) is_moved = move_out(combined).first;
	}
	if (!is_moved) {
		const MoveVector move_delta = hit.trace_end - hit.trace_start;
		if (!IsNearlyZero(move_delta, 0.0f)) {
			is_moved = move_out(adjustment + move_delta).first;
			if (!is_moved && Dot(move_delta, adjustment) > 0.0f) is_moved = move_out(move_delta).first;
		}
	}
	return is_moved;
}

// 면을 따라 미끄러진다. 두 번째 면에 닿으면 두 면에 맞춰 방향을 한 번 더 바꾼다. 간 비율과 마지막 이동 결과를 돌려준다.
MoveSimulation::SlideResult MoveSimulation::SlideAlongSurface(const MoveVector& delta, float time, const MoveVector& normal, const MoveHit& hit)
{
	if (!hit.is_blocking) return { 0.0f, hit };
	const MoveVector slide_normal = is_walking_ ? GetWalkingSlideNormal(normal, hit, delta, floor_) : normal;
	MoveVector slide = ComputeSlideVector(delta, time, slide_normal);
	if (!is_walking_) slide = HandleSlopeBoosting(slide, delta, time, slide_normal);
	if (Dot(slide, delta) <= 0.0f) return { 0.0f, hit };
	MoveHit slide_hit = SafeMove(slide);
	const float first_percent = slide_hit.time;
	float percent = first_percent;
	if (slide_hit.is_blocking && !slide_hit.is_start_penetrating) {
		slide = TwoWallAdjust(slide, slide_hit, slide_normal, is_walking_, floor_);
		if (!IsNearlyZero(slide, MOVE_MIN_SLIDE_DISTANCE) && Dot(slide, delta) > 0.0f) {
			slide_hit = SafeMove(slide);
			percent += slide_hit.time * (1.0f - first_percent);
		}
	}
	return { std::clamp(percent, 0.0f, 1.0f), slide_hit };
}

// 바닥과의 거리가 범위를 벗어났으면 가운데로 맞춘다. 맞춘 결과로 바닥 거리를 고친다.
void MoveSimulation::AdjustFloorHeight()
{
	const float move = GetFloorHeightAdjustment(floor_);
	if (move == 0.0f) return;
	const float old_distance = floor_.is_line_trace ? floor_.line_distance : floor_.floor_distance;
	const float initial_z = location_.z;
	const MoveHit hit = SafeMove({ 0.0f, 0.0f, move });
	if (!hit.is_blocking || hit.is_start_penetrating) {
		floor_.floor_distance += move;
	}
	else if (move > 0.0f) {
		floor_.floor_distance += location_.z - initial_z;
	}
	else {
		floor_.floor_distance = location_.z - hit.location.z;
		if (IsWalkable(hit)) floor_ = MakeSweepFloor(hit, floor_.floor_distance, true);
	}
	// 바닥에 파고들었다가 빠져나온 높이는 속도로 치지 않는다.
	if (old_distance < 0.0f) is_just_teleported_ = true;
}

bool MoveSimulation::IsValidLandingSpot(const MoveVector& location, const MoveHit& hit) const
{
	return IsLandingHitAcceptable(hit) && FindFloor(location, &hit).is_walkable;
}

// 턱 높이만큼 올라 앞으로 간 뒤 다시 내려 디딘다. 위가 막혔거나, 앞으로 전혀 못 갔거나, 디딘 곳이 받아들일 수 없으면 처음 자리로 되돌린다.
MoveSimulation::StepUpResult MoveSimulation::StepUp(const MoveVector& delta, const MoveHit& hit)
{
	const MoveStepUpStart start = GetStepUpStart(location_, hit, floor_, is_walking_);
	if (!start.can_step_up) return { false, std::nullopt };
	const MoveVector old_location = location_;
	const auto revert = [&]()
	{
		location_ = old_location;
		return StepUpResult{ false, std::nullopt };
	};
	const float step_side_z = hit.impact_normal.z;
	if (Move({ 0.0f, 0.0f, start.up_height }, true).is_start_penetrating) return revert();
	MoveHit forward_hit = Move(delta, true);
	if (forward_hit.is_blocking) {
		if (forward_hit.is_start_penetrating) return revert();
		const float forward_time = forward_hit.time;
		const SlideResult slide = SlideAlongSurface(delta, 1.0f - forward_hit.time, forward_hit.normal, forward_hit);
		if (forward_time == 0.0f && slide.percent == 0.0f) return revert();
	}
	const MoveHit down_hit = Move(Down(start.down_height), true);
	if (down_hit.is_start_penetrating) return revert();
	std::optional<MoveFloor> floor;
	if (down_hit.is_blocking) {
		if (!IsStepDownAcceptable(down_hit, delta, old_location, start.floor_point_z)) return revert();
		floor = FindFloor(location_, &down_hit);
		// 처음보다 높이 올라섰는데 바닥이 없고 막은 면이 거의 세운 벽이면, 걸칠 수 없는 턱이라 오르지 않고 미끄러진다.
		if (down_hit.location.z > old_location.z && !floor->is_blocking && step_side_z < MOVE_MAX_STEP_SIDE_Z) return revert();
	}
	return { true, floor };
}

// 걸을 수 있는 비탈이면 비탈을 따라 간다. 다른 비탈에 닿으면 그 비탈을 따라 남은 만큼 가고, 막히면 턱을 올라 보고 못 오르면 면을 따라 미끄러진다.
// 서버 충돌에는 오를 수 없게 표시한 물체가 따로 없으므로 막히면 늘 턱 오르기를 해 본다.
std::optional<MoveFloor> MoveSimulation::MoveAlongFloor(const MoveVector& delta)
{
	if (!floor_.is_walkable) return std::nullopt;
	MoveHit hit = SafeMove(ComputeGroundMovementDelta(delta, floor_.hit, floor_.is_line_trace));
	if (hit.is_start_penetrating) {
		// 빠져나오지 못했으면 면을 따라 미끄러져 본다. 그래도 겹쳐 있으면 이번 위치 변화로 속도를 정하지 않는다.
		if (SlideAlongSurface(delta, 1.0f, hit.normal, hit).hit.is_start_penetrating) is_just_teleported_ = true;
		return std::nullopt;
	}
	if (!hit.is_blocking) return std::nullopt;
	float percent = hit.time;
	if (hit.time > 0.0f && hit.normal.z > MOVE_SMALL_NUMBER && IsWalkable(hit)) {
		const float remaining_percent = 1.0f - percent;
		hit = SafeMove(ComputeGroundMovementDelta(delta * remaining_percent, hit, false));
		percent = std::clamp(percent + hit.time * remaining_percent, 0.0f, 1.0f);
	}
	if (!hit.is_blocking || hit.is_start_penetrating) return std::nullopt;
	const StepUpResult step = StepUp(delta * (1.0f - percent), hit);
	if (step.is_stepped) return step.floor;
	SlideAlongSurface(delta, 1.0f - percent, hit.normal, hit);
	return std::nullopt;
}

// old_location에서 side_step만큼 옆으로 가서 설 수 있는지. 가는 길이 막히지 않으면 그 자리 아래 턱 높이 안에 걸을 수 있는 바닥이 있어야 한다.
bool MoveSimulation::CheckLedgeDirection(const MoveVector& old_location, const MoveVector& side_step) const
{
	const MoveVector side_destination = old_location + side_step;
	MoveHit result = SweepWorld(*map_, old_location, side_destination, CHARACTER_CAPSULE_RADIUS, CHARACTER_CAPSULE_HALF_HEIGHT, false);
	if (result.is_blocking && !IsWalkable(result)) return false;
	if (!result.is_blocking) result = SweepWorld(*map_, side_destination, side_destination + Down(MOVE_MAX_STEP_HEIGHT + MOVE_LEDGE_CHECK_THRESHOLD), CHARACTER_CAPSULE_RADIUS, CHARACTER_CAPSULE_HALF_HEIGHT, false);
	return result.time < 1.0f && IsWalkable(result);
}

// 바닥 끝에서 떨어지지 않고 갈 이동. 가장자리 방향을 알면 가장자리를 따라 가 보고, 모르면 이동 방향의 왼쪽·오른쪽을 차례로 본다. 갈 곳이 없으면 영 벡터다.
MoveVector MoveSimulation::GetLedgeMove(const MoveVector& old_location, const MoveVector& delta, const MoveFloor& old_floor) const
{
	if (IsNearlyZero(delta, 0.0f)) return {};
	// 서 있던 바닥으로 가장자리를 알 수 없으면 지금 바닥으로 본다.
	const MoveVector ledge_normal = GetLedgeNormal(old_floor.is_walkable ? old_floor : floor_);
	if (!IsNearlyZero(ledge_normal, 0.0f)) {
		const MoveVector ledge_move = ComputeSlideVector(delta, 1.0f, -ledge_normal);
		return CheckLedgeDirection(old_location, ledge_move) ? ledge_move : MoveVector{};
	}
	for (const bool is_left : { true, false }) {
		const MoveVector side = GetLedgeSideMove(delta, is_left);
		if (CheckLedgeDirection(old_location, side)) return side;
	}
	return {};
}

// 떨어지기 시작한다. 이번 단계에서 가려던 거리 중 못 간 몫의 시간은 떨어지는 데 쓴다.
void MoveSimulation::StartFalling(int iterations, float remaining, float tick, const MoveVector& delta, const MoveVector& old_location)
{
	const float desired = Length(delta);
	const float actual = Length(Horizontal(location_ - old_location));
	remaining = desired < MOVE_SMALL_NUMBER ? 0.0f : remaining + tick * (1.0f - std::min(1.0f, actual / desired));
	is_walking_ = false;
	floor_ = {};
	StartNewPhysics(remaining, iterations);
}

// 땅에 내려선다. 걷기로 바뀌면 수평 속도만 남기고, 그 자리 바닥을 찾아 높이를 맞춘 뒤 남은 시간을 걷는다.
void MoveSimulation::ProcessLanded(float remaining, int iterations)
{
	is_walking_ = true;
	velocity_ = Horizontal(velocity_);
	floor_ = FindFloor(location_, nullptr);
	AdjustFloorHeight();
	StartNewPhysics(remaining, iterations);
}

void MoveSimulation::StartNewPhysics(float seconds, int iterations)
{
	if (seconds < MOVE_MIN_TICK_TIME || iterations >= MOVE_MAX_SIMULATION_ITERATIONS) return;
	if (is_walking_) PhysWalking(seconds, iterations);
	else PhysFalling(seconds, iterations);
}

// 언리얼 걷기와 같은 순서로 단계마다 속도를 정하고 바닥을 따라 간 뒤 바닥을 다시 찾는다.
// 바닥이 끝나면 가장자리를 따라 가 보고, 갈 곳이 없으면 이번 단계를 되돌려 멈춘다. 서 있던 바닥이 없었거나 움직이지 않았으면 떨어진다.
void MoveSimulation::PhysWalking(float seconds, int iterations)
{
	if (seconds < MOVE_MIN_TICK_TIME) return;
	is_just_teleported_ = false;
	bool has_tried_ledge_move = false;
	float remaining = seconds;
	while (remaining >= MOVE_MIN_TICK_TIME && iterations < MOVE_MAX_SIMULATION_ITERATIONS) {
		++iterations;
		is_just_teleported_ = false;
		const float tick = GetSimulationTimeStep(remaining, iterations);
		remaining -= tick;
		const MoveVector old_location = location_;
		const MoveFloor old_floor = floor_;
		velocity_ = Horizontal(velocity_);
		// 가장자리를 따라 가 보는 단계는 정한 이동 그대로 간다.
		MoveVector delta = velocity_ * tick;
		if (!has_tried_ledge_move) {
			const MoveStep step = CalcWalkStep(velocity_, input_, walk_params_, tick);
			velocity_ = step.velocity;
			delta = step.delta;
		}
		const bool is_zero_delta = IsNearlyZero(delta);
		std::optional<MoveFloor> step_down_floor;
		if (is_zero_delta) remaining = 0.0f;
		else step_down_floor = MoveAlongFloor(delta);
		floor_ = step_down_floor ? *step_down_floor : FindFloor(location_, nullptr);

		if (!floor_.is_walkable) {
			const MoveVector ledge_delta = has_tried_ledge_move ? MoveVector{} : GetLedgeMove(old_location, delta, old_floor);
			if (!IsNearlyZero(ledge_delta, 0.0f)) {
				location_ = old_location;
				floor_ = old_floor;
				has_tried_ledge_move = true;
				velocity_ = ledge_delta * (1.0f / tick);
				remaining += tick;
				--iterations;
				continue;
			}
			if (is_zero_delta || !old_floor.is_walkable) {
				StartFalling(iterations, remaining, tick, delta, old_location);
				return;
			}
			location_ = old_location;
			floor_ = old_floor;
			velocity_ = {};
			break;
		}
		AdjustFloorHeight();
		// 실제로 간 거리가 정한 이동과 다르면 그 차이만큼 속도를 고친다. 막히지 않았으면 정한 속도 그대로다.
		if (!is_just_teleported_) velocity_ = Horizontal(velocity_ + (location_ - old_location - delta) * (1.0f / tick));
		if (IsNearlyZero(location_ - old_location, 0.0f)) break;
	}
	if (is_walking_) velocity_ = Horizontal(velocity_);
}

// 언리얼 떨어지기와 같은 순서로 단계마다 중력으로 움직인다. 막히면 걸을 수 있는 곳이면 내려서고, 아니면 면을 따라 미끄러진다.
// 두 번째 면에도 닿으면 두 면에 맞춰 한 번 더 미끄러지고, 그래도 막히면 내려선 것으로 본다.
void MoveSimulation::PhysFalling(float seconds, int iterations)
{
	if (seconds < MOVE_MIN_TICK_TIME) return;
	float remaining = seconds;
	while (remaining >= MOVE_MIN_TICK_TIME && iterations < MOVE_MAX_SIMULATION_ITERATIONS) {
		++iterations;
		float tick = GetSimulationTimeStep(remaining, iterations);
		remaining -= tick;
		is_just_teleported_ = false;
		const MoveVector old_velocity = velocity_;
		MoveStep step = CalcFallStep(velocity_, tick);
		// 점프 정점을 지나면 정점까지만 가고 남은 시간은 다음 단계로 넘긴다.
		if (old_velocity.z > 0.0f && step.velocity.z <= 0.0f && jump_apex_attempts_ < MOVE_MAX_JUMP_APEX_ATTEMPTS) {
			const float apex_time = old_velocity.z / GRAVITY;
			if (apex_time >= MOVE_JUMP_APEX_MIN_TIME && apex_time < tick) {
				remaining += tick - apex_time;
				tick = apex_time;
				--iterations;
				++jump_apex_attempts_;
				step = CalcFallStep(old_velocity, tick);
				step.velocity.z = 0.0f;
			}
		}
		velocity_ = step.velocity;
		MoveHit hit = SafeMove(step.delta);
		float sub_remaining = tick * (1.0f - hit.time);
		if (hit.is_blocking) {
			if (IsValidLandingSpot(location_, hit)) {
				ProcessLanded(remaining + sub_remaining, iterations);
				return;
			}
			// 미끄러질 이동은 끝 속도로 정한다. 중력의 효과가 미끄러짐에 다 들어간다.
			const MoveVector adjusted = velocity_ * tick;
			// 캡슐 아래쪽이 모서리에 닿았으면 그 위에 걸을 수 있는 면이 있을 수 있다.
			if (!hit.is_start_penetrating && ShouldCheckForValidLandingSpot(location_, hit)) {
				const MoveFloor floor = FindFloor(location_, nullptr);
				if (!floor.is_line_trace && floor.is_walkable && IsValidLandingSpot(location_, floor.hit)) {
					ProcessLanded(remaining + sub_remaining, iterations);
					return;
				}
			}
			const MoveVector old_hit_normal = hit.normal;
			const MoveVector old_hit_impact_normal = hit.impact_normal;
			MoveVector delta = HandleSlopeBoosting(ComputeSlideVector(adjusted, 1.0f - hit.time, old_hit_normal), adjusted, 1.0f - hit.time, old_hit_normal);
			if (sub_remaining > MOVE_SMALL_NUMBER && !is_just_teleported_) velocity_ = delta * (1.0f / sub_remaining);
			if (sub_remaining > MOVE_SMALL_NUMBER && Dot(delta, adjusted) > 0.0f) {
				hit = SafeMove(delta);
				if (hit.is_blocking) {
					sub_remaining *= 1.0f - hit.time;
					if (IsValidLandingSpot(location_, hit)) {
						ProcessLanded(remaining + sub_remaining, iterations);
						return;
					}
					const MoveVector pre_two_wall_delta = delta;
					delta = TwoWallAdjust(delta, hit, old_hit_normal, false, floor_);
					// 예각 구석에서 위로 밀려 올라가지 않게 한다.
					delta = HandleSlopeBoosting(delta, pre_two_wall_delta, 1.0f - hit.time, hit.normal);
					if (sub_remaining > MOVE_SMALL_NUMBER && !is_just_teleported_) velocity_ = delta * (1.0f / sub_remaining);
					// 둘 다 설 수 없는 두 비탈 사이에 끼었는지
					const bool is_ditch = old_hit_impact_normal.z > 0.0f && hit.impact_normal.z > 0.0f && std::fabs(delta.z) <= MOVE_SMALL_NUMBER && Dot(hit.impact_normal, old_hit_impact_normal) < 0.0f;
					hit = SafeMove(delta);
					if (hit.time == 0.0f) {
						// 끼어 움직이지 못하면 옆으로 비켜 본다.
						MoveVector side = SafeNormal(Horizontal(old_hit_normal + hit.impact_normal));
						if (IsNearlyZero(side)) side = SafeNormal({ old_hit_normal.y, -old_hit_normal.x, 0.0f });
						hit = SafeMove(side);
					}
					if (is_ditch || IsValidLandingSpot(location_, hit) || hit.time == 0.0f) {
						ProcessLanded(0.0f, iterations);
						return;
					}
				}
			}
		}
		if (LengthSquared(Horizontal(velocity_)) <= MOVE_SMALL_NUMBER * 10.0f) {
			velocity_.x = 0.0f;
			velocity_.y = 0.0f;
		}
	}
}
