#include "PlayerMovementComponent.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "movement_settings.h"
#include "shared_movement.h"

// 공용 이동 계산은 float으로 계산한다. 위치는 큰 월드 좌표에서 오차가 생기지 않도록 기준 위치(origin)를 뺀 값으로 넘긴다.
static MoveVector ToMove(const FVector& value)
{
	return { static_cast<float>(value.X), static_cast<float>(value.Y), static_cast<float>(value.Z) };
}

static FVector ToVector(const MoveVector& value)
{
	return FVector(value.x, value.y, value.z);
}

static MoveHit ToMoveHit(const FHitResult& hit, const FVector& origin)
{
	MoveHit result;
	result.is_blocking = hit.bBlockingHit;
	result.is_start_penetrating = hit.bStartPenetrating;
	result.time = hit.Time;
	result.trace_start = ToMove(hit.TraceStart - origin);
	result.trace_end = ToMove(hit.TraceEnd - origin);
	result.location = ToMove(hit.Location - origin);
	result.impact_point = ToMove(hit.ImpactPoint - origin);
	result.normal = ToMove(hit.Normal);
	result.impact_normal = ToMove(hit.ImpactNormal);
	result.penetration_depth = hit.PenetrationDepth;
	return result;
}

static MoveFloor ToMoveFloor(const FFindFloorResult& floor, const FVector& origin)
{
	MoveFloor result;
	result.is_blocking = floor.bBlockingHit;
	result.is_walkable = floor.bWalkableFloor;
	result.is_line_trace = floor.bLineTrace;
	result.floor_distance = floor.FloorDist;
	result.line_distance = floor.LineDist;
	result.hit = ToMoveHit(floor.HitResult, origin);
	return result;
}

// 서버 MoveSimulation::PhysWalking과 같은 순서다. 속도와 이번 단계 이동은 공용 계산으로 정하고, 바닥을 따라 간 뒤 바닥을 다시 찾는다.
// 바닥 끝에서 떨어질 수 없으면(다른 플레이어 캐릭터) 가장자리를 따라 가 보고 갈 곳이 없으면 멈추며, 떨어질 수 있으면 떨어지기 시작한다.
void UPlayerMovementComponent::PhysWalking(float delta_time, int32 iterations)
{
	if (delta_time < MOVE_MIN_TICK_TIME)
	{
		return;
	}
	if (!CharacterOwner || (!CharacterOwner->GetController() && !bRunPhysicsWithNoController && CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy))
	{
		Acceleration = FVector::ZeroVector;
		Velocity = FVector::ZeroVector;
		return;
	}
	if (!UpdatedComponent->IsQueryCollisionEnabled())
	{
		SetMovementMode(MOVE_Walking);
		return;
	}
	bJustTeleported = false;
	bool has_checked_fall = false;
	bool has_tried_ledge_move = false;
	float remaining = delta_time;
	// 다른 플레이어 캐릭터를 멈출 자리에 세우는 동안은 마찰과 제동이 꺼져 있으므로 지금 설정값을 쓴다.
	const MoveWalkParams params{ GetMaxSpeed(), GroundFriction, GetMaxBrakingDeceleration() };
	const float max_acceleration = GetMaxAcceleration();
	const MoveVector input = max_acceleration > 0.0f ? Horizontal(ToMove(Acceleration / max_acceleration)) : MoveVector{};
	while (remaining >= MOVE_MIN_TICK_TIME && iterations < MOVE_MAX_SIMULATION_ITERATIONS && CharacterOwner && (CharacterOwner->GetController() || bRunPhysicsWithNoController || CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy))
	{
		++iterations;
		bJustTeleported = false;
		const float tick = ::GetSimulationTimeStep(remaining, iterations);
		remaining -= tick;
		FMovementBaseInterfaceData* const old_base = GetMovementBaseInterfaceData_Mutable();
		const FVector previous_base_location = old_base && old_base->IsValid() ? old_base->GetBodyInstanceOwner()->GetPhysicsOwnerTransform().GetLocation() : FVector::ZeroVector;
		const FVector old_location = UpdatedComponent->GetComponentLocation();
		const FFindFloorResult old_floor = CurrentFloor;
		MaintainHorizontalGroundVelocity();
		// 가장자리를 따라 가 보는 단계는 정한 이동 그대로 간다.
		FVector delta = Velocity * tick;
		if (!has_tried_ledge_move)
		{
			const MoveStep step = CalcWalkStep(ToMove(Velocity), input, params, tick);
			Velocity = ToVector(step.velocity);
			delta = ToVector(step.delta);
		}
		const bool is_zero_delta = delta.IsNearlyZero();
		FStepDownResult step_down_result;
		if (is_zero_delta)
		{
			remaining = 0.0f;
		}
		else
		{
			MoveAlongFloor(delta / tick, tick, &step_down_result);
			if (!IsMovingOnGround())
			{
				// 턱 오르기 도중 다른 이동 방식으로 바뀌었으면 못 간 몫의 시간을 돌려받아 이어 간다.
				const double desired = delta.Size();
				if (desired > UE_KINDA_SMALL_NUMBER)
				{
					const double actual = FVector(UpdatedComponent->GetComponentLocation() - old_location).Size2D();
					remaining += tick * (1.0f - static_cast<float>(FMath::Min(1.0, actual / desired)));
				}
				StartNewPhysics(remaining, iterations);
				return;
			}
		}
		if (step_down_result.bComputedFloor)
		{
			CurrentFloor = step_down_result.FloorResult;
		}
		else
		{
			FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, is_zero_delta, nullptr);
		}

		if (!CanWalkOffLedges() && !CurrentFloor.IsWalkableFloor())
		{
			const FVector ledge_delta = has_tried_ledge_move ? FVector::ZeroVector : GetLedgeMove(old_location, delta, old_floor);
			if (!ledge_delta.IsZero())
			{
				RevertMove(old_location, old_base, previous_base_location, old_floor, false);
				has_tried_ledge_move = true;
				Velocity = ledge_delta / tick;
				remaining += tick;
				--iterations;
				continue;
			}
			// 서 있던 바닥이 없었거나 움직이지 않았으면 떨어지고, 아니면 이번 이동을 되돌리고 멈춘다.
			const bool is_must_jump = is_zero_delta || !old_floor.IsWalkableFloor();
			if ((is_must_jump || !has_checked_fall) && CheckFall(old_floor, CurrentFloor.HitResult, delta, old_location, remaining, tick, iterations, is_must_jump))
			{
				return;
			}
			has_checked_fall = true;
			RevertMove(old_location, old_base, previous_base_location, old_floor, true);
			break;
		}
		if (CurrentFloor.IsWalkableFloor())
		{
			AdjustFloorHeight();
			SetBaseFromFloor(CurrentFloor);
		}
		else if (CurrentFloor.HitResult.bStartPenetrating && remaining <= 0.0f)
		{
			// 바닥 찾기가 처음부터 겹쳐 실패했으면 아래로 내려가지 않고 바닥에서 빠져나온다.
			FHitResult hit(CurrentFloor.HitResult);
			hit.TraceEnd = hit.TraceStart + FVector(0.0, 0.0, MOVE_MAX_FLOOR_DISTANCE);
			ResolvePenetration(GetPenetrationAdjustment(hit), hit, UpdatedComponent->GetComponentQuat());
			bForceNextFloorCheck = true;
		}
		if (!CurrentFloor.IsWalkableFloor() && !CurrentFloor.HitResult.bStartPenetrating)
		{
			const bool is_must_jump = bJustTeleported || is_zero_delta || !old_floor.IsWalkableFloor();
			if ((is_must_jump || !has_checked_fall) && CheckFall(old_floor, CurrentFloor.HitResult, delta, old_location, remaining, tick, iterations, is_must_jump))
			{
				return;
			}
			has_checked_fall = true;
		}
		// 실제로 간 거리가 정한 이동과 다르면 그 차이만큼 속도를 고친다. 막히지 않았으면 정한 속도 그대로다.
		if (IsMovingOnGround() && !bJustTeleported)
		{
			Velocity += (UpdatedComponent->GetComponentLocation() - old_location - delta) / tick;
			MaintainHorizontalGroundVelocity();
		}
		if (UpdatedComponent->GetComponentLocation() == old_location)
		{
			break;
		}
	}
	if (IsMovingOnGround())
	{
		MaintainHorizontalGroundVelocity();
	}
}

// 서버 MoveSimulation::PhysFalling과 같은 순서다. 단계마다 공용 계산으로 중력 이동을 정하고, 막히면 내려설 수 있는지 보고 아니면 면을 따라 미끄러진다.
void UPlayerMovementComponent::PhysFalling(float delta_time, int32 iterations)
{
	if (delta_time < MOVE_MIN_TICK_TIME)
	{
		return;
	}
	float remaining = delta_time;
	while (remaining >= MOVE_MIN_TICK_TIME && iterations < MOVE_MAX_SIMULATION_ITERATIONS)
	{
		++iterations;
		float tick = ::GetSimulationTimeStep(remaining, iterations);
		remaining -= tick;
		const FQuat rotation = UpdatedComponent->GetComponentQuat();
		bJustTeleported = false;
		const MoveVector old_velocity = ToMove(Velocity);
		MoveStep step = CalcFallStep(old_velocity, tick);
		// 점프 정점을 지나면 정점까지만 가고 남은 시간은 다음 단계로 넘긴다.
		if (old_velocity.z > 0.0f && step.velocity.z <= 0.0f && NumJumpApexAttempts < MOVE_MAX_JUMP_APEX_ATTEMPTS)
		{
			const float apex_time = old_velocity.z / GRAVITY;
			if (apex_time >= MOVE_JUMP_APEX_MIN_TIME && apex_time < tick)
			{
				remaining += tick - apex_time;
				tick = apex_time;
				--iterations;
				++NumJumpApexAttempts;
				step = CalcFallStep(old_velocity, tick);
				step.velocity.z = 0.0f;
			}
		}
		Velocity = ToVector(step.velocity);
		if (bNotifyApex && Velocity.Z < 0.0)
		{
			bNotifyApex = false;
			NotifyJumpApex();
		}
		FHitResult hit(1.0f);
		SafeMoveUpdatedComponent(ToVector(step.delta), rotation, true, hit);
		if (!HasValidData())
		{
			return;
		}
		float sub_remaining = tick * (1.0f - hit.Time);
		if (hit.bBlockingHit)
		{
			if (IsValidLandingSpot(UpdatedComponent->GetComponentLocation(), hit))
			{
				remaining += sub_remaining;
				ProcessLanded(hit, remaining, iterations);
				return;
			}
			// 미끄러질 이동은 끝 속도로 정한다. 중력의 효과가 미끄러짐에 다 들어간다.
			const FVector adjusted = Velocity * tick;
			// 캡슐 아래쪽이 모서리에 닿았으면 그 위에 걸을 수 있는 면이 있을 수 있다.
			if (!hit.bStartPenetrating && ShouldCheckForValidLandingSpot(tick, adjusted, hit))
			{
				const FVector location = UpdatedComponent->GetComponentLocation();
				FFindFloorResult floor;
				FindFloor(location, floor, false);
				if (!floor.bLineTrace && floor.IsWalkableFloor() && IsValidLandingSpot(location, floor.HitResult))
				{
					remaining += sub_remaining;
					ProcessLanded(floor.HitResult, remaining, iterations);
					return;
				}
			}
			HandleImpact(hit, tick, adjusted);
			if (!HasValidData() || !IsFalling())
			{
				return;
			}
			const FVector old_hit_normal = hit.Normal;
			const FVector old_hit_impact_normal = hit.ImpactNormal;
			FVector delta = ComputeSlideVector(adjusted, 1.0f - hit.Time, old_hit_normal, hit);
			if (sub_remaining > UE_KINDA_SMALL_NUMBER && !bJustTeleported)
			{
				Velocity = delta / sub_remaining;
			}
			if (sub_remaining > UE_KINDA_SMALL_NUMBER && (delta | adjusted) > 0.0)
			{
				SafeMoveUpdatedComponent(delta, rotation, true, hit);
				if (hit.bBlockingHit)
				{
					const float last_move_time = sub_remaining;
					sub_remaining *= 1.0f - hit.Time;
					if (IsValidLandingSpot(UpdatedComponent->GetComponentLocation(), hit))
					{
						remaining += sub_remaining;
						ProcessLanded(hit, remaining, iterations);
						return;
					}
					HandleImpact(hit, last_move_time, delta);
					if (!HasValidData() || !IsFalling())
					{
						return;
					}
					const FVector pre_two_wall_delta = delta;
					TwoWallAdjust(delta, hit, old_hit_normal);
					// 예각 구석에서 위로 밀려 올라가지 않게 한다.
					delta = HandleSlopeBoosting(delta, pre_two_wall_delta, 1.0f - hit.Time, hit.Normal, hit);
					if (sub_remaining > UE_KINDA_SMALL_NUMBER && !bJustTeleported)
					{
						Velocity = delta / sub_remaining;
					}
					// 둘 다 설 수 없는 두 비탈 사이에 끼었는지
					const bool is_ditch = old_hit_impact_normal.Z > 0.0 && hit.ImpactNormal.Z > 0.0 && FMath::Abs(delta.Z) <= UE_KINDA_SMALL_NUMBER && (hit.ImpactNormal | old_hit_impact_normal) < 0.0;
					SafeMoveUpdatedComponent(delta, rotation, true, hit);
					if (hit.Time == 0.0f)
					{
						// 끼어 움직이지 못하면 옆으로 비켜 본다.
						FVector side = FVector(old_hit_normal + hit.ImpactNormal).GetSafeNormal2D();
						if (side.IsNearlyZero())
						{
							side = FVector(old_hit_normal.Y, -old_hit_normal.X, 0.0).GetSafeNormal();
						}
						SafeMoveUpdatedComponent(side, rotation, true, hit);
					}
					if (is_ditch || IsValidLandingSpot(UpdatedComponent->GetComponentLocation(), hit) || hit.Time == 0.0f)
					{
						remaining = 0.0f;
						ProcessLanded(hit, remaining, iterations);
						return;
					}
				}
			}
		}
		if (Velocity.SizeSquared2D() <= UE_KINDA_SMALL_NUMBER * 10.0)
		{
			Velocity.X = 0.0;
			Velocity.Y = 0.0;
		}
	}
}

// 서버 MoveSimulation::MoveAlongFloor와 같은 순서다. 걸을 수 있는 비탈이면 비탈을 따라 가고, 다른 비탈에 닿으면 그 비탈을 따라 남은 만큼 간다.
// 막히면 턱을 올라 보고, 못 오르면 면을 따라 미끄러진다.
void UPlayerMovementComponent::MoveAlongFloor(const FVector& in_velocity, float delta_seconds, FStepDownResult* out_step_down_result)
{
	if (!CurrentFloor.IsWalkableFloor())
	{
		return;
	}
	const FVector delta = FVector(in_velocity.X, in_velocity.Y, 0.0) * delta_seconds;
	FHitResult hit(1.0f);
	FVector ramp = ComputeGroundMovementDelta(delta, CurrentFloor.HitResult, CurrentFloor.bLineTrace);
	SafeMoveUpdatedComponent(ramp, UpdatedComponent->GetComponentQuat(), true, hit);
	float last_move_time = delta_seconds;
	if (hit.bStartPenetrating)
	{
		// 빠져나오지 못했으면 면을 따라 미끄러져 본다. 그래도 겹쳐 있으면 이번 위치 변화로 속도를 정하지 않는다.
		HandleImpact(hit);
		SlideAlongSurface(delta, 1.0f, hit.Normal, hit, true);
		if (hit.bStartPenetrating)
		{
			OnCharacterStuckInGeometry(&hit);
		}
		return;
	}
	if (!hit.IsValidBlockingHit())
	{
		return;
	}
	float percent = hit.Time;
	if (hit.Time > 0.0f && hit.Normal.Z > MOVE_SMALL_NUMBER && IsWalkable(hit))
	{
		const float remaining_percent = 1.0f - percent;
		ramp = ComputeGroundMovementDelta(delta * remaining_percent, hit, false);
		last_move_time *= remaining_percent;
		SafeMoveUpdatedComponent(ramp, UpdatedComponent->GetComponentQuat(), true, hit);
		percent = FMath::Clamp(percent + hit.Time * remaining_percent, 0.0f, 1.0f);
	}
	if (!hit.IsValidBlockingHit())
	{
		return;
	}
	const UObject* const base_object = GetMovementBaseObject();
	if (CanStepUp(hit) || (base_object && hit.GetComponent() == base_object))
	{
		if (!StepUp(GetGravityDirection(), delta * (1.0f - percent), hit, out_step_down_result))
		{
			HandleImpact(hit, last_move_time, ramp);
			SlideAlongSurface(delta, 1.0f - percent, hit.Normal, hit, true);
		}
	}
	else if (hit.GetComponent() && !hit.GetComponent()->CanCharacterStepUp(CharacterOwner))
	{
		HandleImpact(hit, last_move_time, ramp);
		SlideAlongSurface(delta, 1.0f - percent, hit.Normal, hit, true);
	}
}

// 서버 MoveSimulation::StepUp과 같은 순서다. 턱 높이만큼 올라 앞으로 간 뒤 다시 내려 디딘다.
// 위가 막혔거나, 앞으로 전혀 못 갔거나, 디딘 곳이 받아들일 수 없으면 처음 자리로 되돌린다. 계산은 처음 자리를 기준 위치로 한다.
bool UPlayerMovementComponent::StepUp(const FVector& gravity_direction, const FVector& delta, const FHitResult& hit, FStepDownResult* out_step_down_result)
{
	if (!CanStepUp(hit) || MaxStepHeight <= 0.0f)
	{
		return false;
	}
	const FVector old_location = UpdatedComponent->GetComponentLocation();
	const MoveStepUpStart start = GetStepUpStart(MoveVector{}, ToMoveHit(hit, old_location), ToMoveFloor(CurrentFloor, old_location), IsMovingOnGround());
	if (!start.can_step_up)
	{
		return false;
	}
	FScopedMovementUpdate scoped_step_up(UpdatedComponent, EScopedUpdate::DeferredUpdates);
	const FQuat rotation = UpdatedComponent->GetComponentQuat();
	FHitResult up_hit(1.0f);
	MoveUpdatedComponent(FVector(0.0, 0.0, start.up_height), rotation, true, &up_hit);
	if (up_hit.bStartPenetrating)
	{
		scoped_step_up.RevertMove();
		return false;
	}
	FHitResult forward_hit(1.0f);
	MoveUpdatedComponent(delta, rotation, true, &forward_hit);
	if (forward_hit.bBlockingHit)
	{
		if (forward_hit.bStartPenetrating)
		{
			scoped_step_up.RevertMove();
			return false;
		}
		if (up_hit.bBlockingHit)
		{
			HandleImpact(up_hit);
		}
		HandleImpact(forward_hit);
		if (IsFalling())
		{
			return true;
		}
		const float forward_time = forward_hit.Time;
		const float slide_percent = SlideAlongSurface(delta, 1.0f - forward_hit.Time, forward_hit.Normal, forward_hit, true);
		if (IsFalling() || (forward_time == 0.0f && slide_percent == 0.0f))
		{
			scoped_step_up.RevertMove();
			return false;
		}
	}
	FHitResult down_hit(1.0f);
	MoveUpdatedComponent(FVector(0.0, 0.0, -start.down_height), UpdatedComponent->GetComponentQuat(), true, &down_hit);
	if (down_hit.bStartPenetrating)
	{
		scoped_step_up.RevertMove();
		return false;
	}
	FStepDownResult step_down_result;
	if (down_hit.IsValidBlockingHit())
	{
		const MoveHit relative_down_hit = ToMoveHit(down_hit, old_location);
		if (!IsStepDownAcceptable(relative_down_hit, ToMove(delta), MoveVector{}, start.floor_point_z) || (relative_down_hit.impact_point.z - start.floor_point_z > 0.0f && !CanStepUp(down_hit)))
		{
			scoped_step_up.RevertMove();
			return false;
		}
		if (out_step_down_result)
		{
			FindFloor(UpdatedComponent->GetComponentLocation(), step_down_result.FloorResult, false, &down_hit);
			// 처음보다 높이 올라섰는데 바닥이 없고 막은 면이 거의 세운 벽이면, 걸칠 수 없는 턱이라 오르지 않고 미끄러진다.
			if (down_hit.Location.Z > old_location.Z && !step_down_result.FloorResult.bBlockingHit && hit.ImpactNormal.Z < MOVE_MAX_STEP_SIDE_Z)
			{
				scoped_step_up.RevertMove();
				return false;
			}
			step_down_result.bComputedFloor = true;
		}
	}
	if (out_step_down_result)
	{
		*out_step_down_result = step_down_result;
	}
	return true;
}

// 서버 MoveSimulation::SlideAlongSurface와 같은 순서다. 면을 따라 미끄러지고, 두 번째 면에 닿으면 두 면에 맞춰 방향을 한 번 더 바꾼다.
float UPlayerMovementComponent::SlideAlongSurface(const FVector& delta, float time, const FVector& normal, FHitResult& hit, bool is_handling_impact)
{
	if (!hit.bBlockingHit)
	{
		return 0.0f;
	}
	const FVector origin = UpdatedComponent->GetComponentLocation();
	const FVector slide_normal = IsMovingOnGround() ? ToVector(GetWalkingSlideNormal(ToMove(normal), ToMoveHit(hit, origin), ToMove(delta), ToMoveFloor(CurrentFloor, origin))) : normal;
	FVector slide = ComputeSlideVector(delta, time, slide_normal, hit);
	if ((slide | delta) <= 0.0)
	{
		return 0.0f;
	}
	const FQuat rotation = UpdatedComponent->GetComponentQuat();
	SafeMoveUpdatedComponent(slide, rotation, true, hit);
	const float first_percent = hit.Time;
	float percent = first_percent;
	if (hit.IsValidBlockingHit())
	{
		if (is_handling_impact)
		{
			HandleImpact(hit, first_percent * time, slide);
		}
		TwoWallAdjust(slide, hit, slide_normal);
		if (!slide.IsNearlyZero(MOVE_MIN_SLIDE_DISTANCE) && (slide | delta) > 0.0)
		{
			SafeMoveUpdatedComponent(slide, rotation, true, hit);
			const float second_percent = hit.Time * (1.0f - first_percent);
			percent += second_percent;
			if (is_handling_impact && hit.bBlockingHit)
			{
				HandleImpact(hit, second_percent * time, slide);
			}
		}
	}
	return FMath::Clamp(percent, 0.0f, 1.0f);
}

void UPlayerMovementComponent::TwoWallAdjust(FVector& world_space_delta, const FHitResult& hit, const FVector& old_hit_normal) const
{
	const FVector origin = UpdatedComponent->GetComponentLocation();
	world_space_delta = ToVector(::TwoWallAdjust(ToMove(world_space_delta), ToMoveHit(hit, origin), ToMove(old_hit_normal), IsMovingOnGround(), ToMoveFloor(CurrentFloor, origin)));
}

// 공중에서 미끄러질 때는 원래보다 높이 올라가지 않게 한다.
FVector UPlayerMovementComponent::ComputeSlideVector(const FVector& delta, const float time, const FVector& normal, const FHitResult& hit) const
{
	const FVector result = ToVector(::ComputeSlideVector(ToMove(delta), time, ToMove(normal)));
	return IsFalling() ? HandleSlopeBoosting(result, delta, time, normal, hit) : result;
}

FVector UPlayerMovementComponent::HandleSlopeBoosting(const FVector& slide_result, const FVector& delta, const float time, const FVector& normal, const FHitResult& hit) const
{
	return ToVector(::HandleSlopeBoosting(ToMove(slide_result), ToMove(delta), time, ToMove(normal)));
}

FVector UPlayerMovementComponent::ComputeGroundMovementDelta(const FVector& delta, const FHitResult& ramp_hit, const bool is_hit_from_line_trace) const
{
	return ToVector(::ComputeGroundMovementDelta(ToMove(delta), ToMoveHit(ramp_hit, ramp_hit.TraceStart), is_hit_from_line_trace));
}

FVector UPlayerMovementComponent::GetPenetrationAdjustment(const FHitResult& hit) const
{
	return ToVector(::GetPenetrationAdjustment(ToMoveHit(hit, hit.TraceStart)));
}

bool UPlayerMovementComponent::IsWalkable(const FHitResult& hit) const
{
	return ::IsWalkable(ToMoveHit(hit, hit.TraceStart));
}

bool UPlayerMovementComponent::IsWithinEdgeTolerance(const FVector& capsule_location, const FVector& test_impact_point, const float capsule_radius) const
{
	return ::IsWithinEdgeTolerance(MoveVector{}, ToMove(test_impact_point - capsule_location), capsule_radius);
}

// 서버 MoveSimulation::ComputeFloorDist와 같은 순서다. 아래로 쓸 때는 캡슐 원기둥 부분을 줄이고, 가장자리에 닿았거나 처음부터 겹쳤으면 더 줄여 다시 쓴다.
// 쓸기가 걸을 수 없는 면에 닿았으면 캡슐 중심에서 선분으로 다시 본다.
void UPlayerMovementComponent::ComputeFloorDist(const FVector& capsule_location, float line_distance, float sweep_distance, FFindFloorResult& out_floor_result, float sweep_radius, const FHitResult* downward_sweep_result) const
{
	out_floor_result.Clear();
	float pawn_radius = 0.0f;
	float pawn_half_height = 0.0f;
	CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(pawn_radius, pawn_half_height);
	bool is_skipping_sweep = false;
	if (downward_sweep_result && downward_sweep_result->IsValidBlockingHit())
	{
		// 방금 쓴 것이 아래로 곧게 쓴 것이면 그 결과를 바닥으로 쓴다.
		const FVector trace = downward_sweep_result->TraceStart - downward_sweep_result->TraceEnd;
		if (trace.Z > 0.0 && trace.SizeSquared2D() <= UE_KINDA_SMALL_NUMBER && IsWithinEdgeTolerance(downward_sweep_result->Location, downward_sweep_result->ImpactPoint, pawn_radius))
		{
			is_skipping_sweep = true;
			const bool is_walkable = IsWalkable(*downward_sweep_result);
			out_floor_result.SetFromSweep(*downward_sweep_result, static_cast<float>(capsule_location.Z - downward_sweep_result->Location.Z), is_walkable);
			if (is_walkable)
			{
				return;
			}
		}
	}
	if (sweep_distance < line_distance)
	{
		return;
	}
	FCollisionQueryParams query_params(SCENE_QUERY_STAT(ComputeFloorDist), false, CharacterOwner);
	query_params.bTraceIntoSubComponents = true;
	query_params.bReplaceHitWithSubComponents = false;
	FCollisionResponseParams response_params;
	InitCollisionParams(query_params, response_params);
	const ECollisionChannel channel = UpdatedComponent->GetCollisionObjectType();

	if (!is_skipping_sweep && sweep_distance > 0.0f && sweep_radius > 0.0f)
	{
		const float cylinder = pawn_half_height - pawn_radius;
		float shrink_height = cylinder * (1.0f - MOVE_FLOOR_SWEEP_SHRINK_SCALE);
		float trace_distance = sweep_distance + shrink_height;
		FCollisionShape capsule = FCollisionShape::MakeCapsule(sweep_radius, pawn_half_height - shrink_height);
		FHitResult hit(1.0f);
		if (FloorSweepTest(hit, capsule_location, capsule_location + FVector(0.0, 0.0, -trace_distance), channel, capsule, query_params, response_params))
		{
			if (hit.bStartPenetrating || !IsWithinEdgeTolerance(capsule_location, hit.ImpactPoint, capsule.Capsule.Radius))
			{
				capsule.Capsule.Radius = FMath::Max(0.0f, capsule.Capsule.Radius - MOVE_SWEEP_EDGE_REJECT_DISTANCE - MOVE_SMALL_NUMBER);
				if (!capsule.IsNearlyZero())
				{
					shrink_height = cylinder * (1.0f - MOVE_FLOOR_SWEEP_RETRY_SHRINK_SCALE);
					trace_distance = sweep_distance + shrink_height;
					capsule.Capsule.HalfHeight = FMath::Max(pawn_half_height - shrink_height, capsule.Capsule.Radius);
					hit.Reset(1.0f, false);
					FloorSweepTest(hit, capsule_location, capsule_location + FVector(0.0, 0.0, -trace_distance), channel, capsule, query_params, response_params);
				}
			}
			// 바닥 거리는 음수일 수 있다. 바닥에 파고든 만큼 위로 빠져나오는 데 쓴다.
			const float sweep_result = GetSweepFloorDistance(hit.Time, trace_distance, shrink_height);
			out_floor_result.SetFromSweep(hit, sweep_result, false);
			if (hit.IsValidBlockingHit() && IsWalkable(hit) && sweep_result <= sweep_distance)
			{
				out_floor_result.bWalkableFloor = true;
				return;
			}
		}
	}

	// 쓸기가 아무것에도 닿지 않았으면 선분 검사도 하지 않는다. 처음부터 겹쳤으면 선분으로 다시 본다.
	if (!out_floor_result.bBlockingHit && !out_floor_result.HitResult.bStartPenetrating)
	{
		out_floor_result.FloorDist = sweep_distance;
		return;
	}
	if (line_distance > 0.0f)
	{
		// 캡슐 중심에서 아래로 본다. 캡슐 반 높이만큼 위에서 시작하므로 그만큼 뺀다.
		const float trace_distance = line_distance + pawn_half_height;
		FHitResult hit(1.0f);
		if (GetWorld()->LineTraceSingleByChannel(hit, capsule_location, capsule_location + FVector(0.0, 0.0, -trace_distance), channel, query_params, response_params) && hit.Time > 0.0f)
		{
			const float line_result = GetSweepFloorDistance(hit.Time, trace_distance, pawn_half_height);
			out_floor_result.bBlockingHit = true;
			if (line_result <= line_distance && IsWalkable(hit))
			{
				out_floor_result.SetFromLineTrace(hit, out_floor_result.FloorDist, line_result, true);
				return;
			}
		}
	}
	out_floor_result.bWalkableFloor = false;
}

// 서버 MoveSimulation::AdjustFloorHeight와 같은 순서다. 바닥과의 거리가 범위를 벗어났으면 가운데로 맞추고, 맞춘 결과로 바닥 거리를 고친다.
void UPlayerMovementComponent::AdjustFloorHeight()
{
	const FVector origin = UpdatedComponent->GetComponentLocation();
	const float move = GetFloorHeightAdjustment(ToMoveFloor(CurrentFloor, origin));
	if (move == 0.0f)
	{
		return;
	}
	const float old_distance = CurrentFloor.GetDistanceToFloor();
	FHitResult hit(1.0f);
	SafeMoveUpdatedComponent(FVector(0.0, 0.0, move), UpdatedComponent->GetComponentQuat(), true, hit);
	if (!hit.IsValidBlockingHit())
	{
		CurrentFloor.FloorDist += move;
	}
	else if (move > 0.0f)
	{
		CurrentFloor.FloorDist += static_cast<float>(UpdatedComponent->GetComponentLocation().Z - origin.Z);
	}
	else
	{
		CurrentFloor.FloorDist = static_cast<float>(UpdatedComponent->GetComponentLocation().Z - hit.Location.Z);
		if (IsWalkable(hit))
		{
			CurrentFloor.SetFromSweep(hit, CurrentFloor.FloorDist, true);
		}
	}
	// 바닥에 파고들었다가 빠져나온 높이는 속도로 치지 않는다.
	bJustTeleported |= old_distance < 0.0f;
	// 높이를 맞췄으면 다음 프레임에 바닥을 다시 찾는다.
	if (CharacterOwner && CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
	{
		bForceNextFloorCheck = true;
	}
}

bool UPlayerMovementComponent::IsValidLandingSpot(const FVector& capsule_location, const FHitResult& hit) const
{
	if (!IsLandingHitAcceptable(ToMoveHit(hit, capsule_location)))
	{
		return false;
	}
	FFindFloorResult floor;
	FindFloor(capsule_location, floor, false, &hit);
	return floor.IsWalkableFloor();
}

bool UPlayerMovementComponent::ShouldCheckForValidLandingSpot(float delta_time, const FVector& delta, const FHitResult& hit) const
{
	return ::ShouldCheckForValidLandingSpot(MoveVector{}, ToMoveHit(hit, UpdatedComponent->GetComponentLocation()));
}

// 서버 MoveSimulation::GetLedgeMove와 같은 순서다. 가장자리 방향을 알면 가장자리를 따라 가 보고, 모르면 이동 방향의 왼쪽·오른쪽을 차례로 본다.
FVector UPlayerMovementComponent::GetLedgeMove(const FVector& old_location, const FVector& delta, const FFindFloorResult& old_floor) const
{
	if (!HasValidData() || delta.IsZero())
	{
		return FVector::ZeroVector;
	}
	// 서 있던 바닥으로 가장자리를 알 수 없으면 지금 바닥으로 본다.
	const MoveVector ledge_normal = GetLedgeNormal(ToMoveFloor(old_floor.IsWalkableFloor() ? old_floor : CurrentFloor, old_location));
	if (!IsNearlyZero(ledge_normal, 0.0f))
	{
		const FVector ledge_move = ToVector(::ComputeSlideVector(ToMove(delta), 1.0f, -ledge_normal));
		return CheckLedgeDirection(old_location, ledge_move, old_floor) ? ledge_move : FVector::ZeroVector;
	}
	for (const bool is_left : { true, false })
	{
		const FVector side = ToVector(GetLedgeSideMove(ToMove(delta), is_left));
		if (CheckLedgeDirection(old_location, side, old_floor))
		{
			return side;
		}
	}
	return FVector::ZeroVector;
}
