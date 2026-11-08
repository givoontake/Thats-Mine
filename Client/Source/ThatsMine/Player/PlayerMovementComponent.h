#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PlayerMovementComponent.generated.h"

// 플레이어 캐릭터(내 캐릭터와 다른 플레이어 캐릭터)의 걷기·떨어지기. 언리얼 캐릭터 이동과 같은 순서로 단계를 진행하되,
// 단계마다 속도·미끄러짐·턱 오르기·바닥 거리·착지 판단은 서버와 같은 공용 이동 계산(SharedMovement)으로 하고, 충돌은 엔진(Chaos)에 묻는다.
// 서버 MoveSimulation과 단계 순서가 같아야 한다. 계산이 들어 있는 엔진 함수는 공용 계산을 쓰도록 바꾸고, 충돌을 묻고 위치를 옮기는 함수는 엔진 것을 그대로 쓴다.
UCLASS()
class THATSMINE_API UPlayerMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	virtual void AdjustFloorHeight() override;
	virtual bool StepUp(const FVector& gravity_direction, const FVector& delta, const FHitResult& hit, FStepDownResult* out_step_down_result = nullptr) override;
	virtual void PhysFalling(float delta_time, int32 iterations) override;
	virtual FVector GetLedgeMove(const FVector& old_location, const FVector& delta, const FFindFloorResult& old_floor) const override;
	virtual bool IsWalkable(const FHitResult& hit) const override;
	virtual bool IsWithinEdgeTolerance(const FVector& capsule_location, const FVector& test_impact_point, const float capsule_radius) const override;
	virtual void ComputeFloorDist(const FVector& capsule_location, float line_distance, float sweep_distance, FFindFloorResult& out_floor_result, float sweep_radius, const FHitResult* downward_sweep_result = nullptr) const override;
	virtual bool IsValidLandingSpot(const FVector& capsule_location, const FHitResult& hit) const override;
	virtual bool ShouldCheckForValidLandingSpot(float delta_time, const FVector& delta, const FHitResult& hit) const override;

protected:
	virtual void PhysWalking(float delta_time, int32 iterations) override;
	virtual FVector ComputeGroundMovementDelta(const FVector& delta, const FHitResult& ramp_hit, const bool is_hit_from_line_trace) const override;
	virtual void MoveAlongFloor(const FVector& in_velocity, float delta_seconds, FStepDownResult* out_step_down_result = nullptr) override;
	virtual FVector GetPenetrationAdjustment(const FHitResult& hit) const override;
	virtual float SlideAlongSurface(const FVector& delta, float time, const FVector& normal, FHitResult& hit, bool is_handling_impact) override;
	virtual void TwoWallAdjust(FVector& world_space_delta, const FHitResult& hit, const FVector& old_hit_normal) const override;
	virtual FVector ComputeSlideVector(const FVector& delta, const float time, const FVector& normal, const FHitResult& hit) const override;
	virtual FVector HandleSlopeBoosting(const FVector& slide_result, const FVector& delta, const float time, const FVector& normal, const FHitResult& hit) const override;
};
