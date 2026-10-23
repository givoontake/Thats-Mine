#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "remote_move_state.h"
#include "RemoteMovementComponent.generated.h"

// 다른 플레이어 캐릭터에 붙어, 받은 입력과 몸 방향으로 로컬 이동 계산을 이어 간다.
// 새 좌표를 받으면 캐릭터 위치와의 차이를 보간 이동으로 메우고, 다음 좌표를 받으면 그 좌표 기준으로 새로 보간한다.
UCLASS()
class THATSMINE_API URemoteMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URemoteMovementComponent();

	void ApplyState(const FRemoteMoveState& state);
	// 받은 이동 키 입력. X가 오른쪽, Y가 앞쪽이다.
	void SetMoveInput(const FVector2D& move_input) { move_input_ = move_input; }
	// 받은 이동 키 입력과 보낸 사람이 키를 바꾼 순간의 위치·수평 속도. 멈춤이면 보낸 사람이 멈춘 자리에 맞춰 세운다.
	void ApplyMoveInput(const FVector2D& move_input, const FVector& location, const FVector2D& sent_velocity);
	void SetAiming(bool is_aiming);
	void SetWeapon(EWeaponType weapon);
	float GetViewPitch() const { return view_pitch_; }

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float delta_seconds, ELevelTick tick_type, FActorComponentTickFunction* this_tick_function) override;

private:
	// 받은 좌표와의 차이를 메우는 시간. 다음 좌표가 올 주기에 맞춘다.
	static constexpr float CORRECTION_SECONDS = 0.1f;
	static constexpr float ROTATION_SMOOTH_SECONDS = 0.05f;
	// 차이가 이보다 크면 보간하지 않고 바로 옮긴다.
	static constexpr float SNAP_DISTANCE = 100.0f;

	FRemoteMoveState state_;
	FVector2D move_input_ = FVector2D::ZeroVector;
	bool has_state_ = false;
	float view_pitch_ = 0.0f;
	// 남은 보간 이동량과 남은 시간
	FVector correction_ = FVector::ZeroVector;
	float correction_remaining_ = 0.0f;

	// 멈출 자리에 맞춰 세우는 기준. 이보다 느리게 걷고 있으면 엔진 제동에 맡긴다.
	static constexpr float STOP_MIN_SPEED = 50.0f;
	// 남은 거리가 이보다 짧으면 멈출 자리에 이미 닿은 것으로 본다.
	static constexpr float STOP_ARRIVE_DISTANCE = 1.0f;

	void UpdateControlledStop(float delta_seconds);
	void FinishControlledStop();
	// 멈출 자리에 맞춰 세우는 중이면 엔진 제동을 끄고 매 프레임 속도를 직접 정한다.
	// is_stop_cruise_: 보낸 사람이 제동을 시작한 위치보다 뒤라서, 지금 속도로 그 위치까지 가는 중
	bool is_controlled_stop_ = false;
	bool is_stop_cruise_ = false;
	FVector stop_start_location_ = FVector::ZeroVector;
	FVector2D stop_direction_ = FVector2D::ZeroVector;
	float stop_sent_speed_ = 0.0f;
	float stop_distance_ = 0.0f;
	float stop_speed_ = 0.0f;
	float stop_duration_ = 0.0f;
	float stop_elapsed_ = 0.0f;
	float saved_braking_deceleration_ = 0.0f;
	float saved_ground_friction_ = 0.0f;
};
