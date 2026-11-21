#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "local_move_step.h"
#include "PlayerMovementComponent.generated.h"

class APlayerCharacter;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnLocalMoveStep, const FLocalMoveStep&);

// 플레이어 캐릭터(내 캐릭터와 다른 플레이어 캐릭터)의 걷기·떨어지기. 언리얼 캐릭터 이동과 같은 순서로 단계를 진행하되,
// 단계마다 속도·미끄러짐·턱 오르기·바닥 거리·착지 판단은 서버와 같은 공용 이동 계산(SharedMovement)으로 하고, 충돌은 엔진(Chaos)에 묻는다.
// 서버 MoveSimulation과 단계 순서가 같아야 한다. 계산이 들어 있는 엔진 함수는 공용 계산을 쓰도록 바꾸고, 충돌을 묻고 위치를 옮기는 함수는 엔진 것을 그대로 쓴다.
// 내 캐릭터는 전송 프레임(MOVE_STEP_MS)마다 그 번호를 계산하고, 그 사이 게임 프레임은 마지막 번호 끝 상태에서 지금 입력으로 이어 계산해 그린다.
// 번호는 스폰을 받은 순간 0에서 시작하고, 게임 프레임마다 처음 읽은 시각으로 정한다. 번호의 입력은 그 번호의 마지막 게임 프레임 입력이고, 점프는 그 번호 안에서 한 번이라도 누르면 넣는다.
// 다른 플레이어 캐릭터는 이동을 계산하지 않는다.
UCLASS()
class THATSMINE_API UPlayerMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	virtual void TickComponent(float delta_time, ELevelTick tick_type, FActorComponentTickFunction* this_tick_function) override;
	// 지금 상태를 번호 상태로 삼고 번호 시계를 지금 시각 0번에서 시작한다. 바닥에서 떠 있으면 떨어지는 상태로 시작한다. 내 캐릭터 스폰을 받은 순간 부른다.
	void StartFrameClock();
	// 이번 게임 프레임의 전송 프레임 번호. 게임 프레임마다 처음 부를 때 시각을 읽어 정하고, 그 게임 프레임 안에서는 같은 번호다.
	uint32 GetFrameNumber() const;
	// 마지막 번호 끝 상태를 바꾼다. 롤백을 받으면 받은 상태를 지금 이동 상태로 세팅할 때 쓴다. 아직 계산하지 않은 지난 번호는 버리고 지금 번호부터 이어 간다.
	void SetStepState(const FVector& location, const FVector& velocity, bool is_falling);
	// 내 캐릭터 이동을 번호 하나 계산할 때마다 알린다.
	FOnLocalMoveStep on_move_step_;

	virtual void AdjustFloorHeight() override;
	virtual bool StepUp(const FVector& gravity_direction, const FVector& delta, const FHitResult& hit, FStepDownResult* out_step_down_result = nullptr) override;
	virtual void PhysFalling(float delta_time, int32 iterations) override;
	virtual FVector GetLedgeMove(const FVector& old_location, const FVector& delta, const FFindFloorResult& old_floor) const override;
	virtual bool IsWalkable(const FHitResult& hit) const override;
	virtual bool IsWithinEdgeTolerance(const FVector& capsule_location, const FVector& test_impact_point, const float capsule_radius) const override;
	virtual void ComputeFloorDist(const FVector& capsule_location, float line_distance, float sweep_distance, FFindFloorResult& out_floor_result, float sweep_radius, const FHitResult* downward_sweep_result = nullptr) const override;
	virtual bool IsValidLandingSpot(const FVector& capsule_location, const FHitResult& hit) const override;
	virtual bool ShouldCheckForValidLandingSpot(float delta_time, const FVector& delta, const FHitResult& hit) const override;

private:
	// 번호 끝 상태. 번호 사이 게임 프레임은 이어 계산한 뒤 다음 게임 프레임에 이 상태로 돌아간다.
	struct FStepState
	{
		FVector location = FVector::ZeroVector;
		FVector velocity = FVector::ZeroVector;
		TEnumAsByte<EMovementMode> movement_mode = MOVE_None;
		FFindFloorResult floor;
		// 캐릭터의 점프 상태. 이어 계산에서 뛴 것을 되돌린다.
		bool is_pressed_jump = false;
		bool was_jumping = false;
		float jump_key_hold_time = 0.0f;
		float jump_force_time_remaining = 0.0f;
		int32 jump_current_count = 0;
		int32 jump_current_count_pre_jump = 0;
	};
	// 한 번호의 이동 입력. 그 번호 안의 마지막 게임 프레임 값이고, 점프는 그 번호 안에서 한 번이라도 누르면 참이다.
	struct FFrameInput
	{
		uint32 frame_number = 0;
		uint8 move_keys = 0;
		float yaw = 0.0f;
		float view_pitch = 0.0f;
		bool is_aiming = false;
		bool is_jump_requested = false;
	};
	FStepState step_state_;
	bool has_step_state_ = false;
	// 0번이 시작한 실제 시각(FPlatformTime 초)
	double frame_start_time_ = 0.0;
	// 시각을 읽은 게임 프레임(GFrameCounter)과 그때 읽은 실제 시각
	mutable uint64 read_frame_counter_ = 0;
	mutable double frame_time_ = 0.0;
	// 다음에 계산할 번호와 그 번호가 시작한 실제 시각
	uint32 step_number_ = 0;
	double step_real_time_ = 0.0;
	// 마지막 게임 프레임이 남긴 그 번호의 이동 입력. 번호가 끝나면 이 입력으로 계산한다.
	FFrameInput frame_input_;
	// 지난 프레임에 이어 계산해 그린 상태. 다음 프레임에 이것과 다르면 그사이 다른 곳에서 바꾼 것이라 그 상태를 단계 상태로 삼는다.
	FVector display_location_ = FVector::ZeroVector;
	FVector display_velocity_ = FVector::ZeroVector;
	TEnumAsByte<EMovementMode> display_movement_mode_ = MOVE_None;

	void SaveStepState();
	void RestoreStepState();
	void SaveDisplayState();
	// 이동 키 move_keys와 조준 여부(와 is_jumping이면 점프)로 delta_time초 이동한다.
	void RunControlledMove(APlayerCharacter* character, float delta_time, uint8 move_keys, bool is_aiming, bool is_jumping, ELevelTick tick_type, FActorComponentTickFunction* this_tick_function);

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
