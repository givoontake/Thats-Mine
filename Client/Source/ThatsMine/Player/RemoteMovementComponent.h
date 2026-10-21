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
	void SetAiming(bool is_aiming);
	void SetWeapon(EWeaponType weapon);
	float GetViewPitch() const { return view_pitch_; }
	// 지금 보이는 이 캐릭터의 시각(ms). 마지막으로 받은 상태의 클라이언트 시각에 받은 뒤 지난 시간을 더한다.
	uint32 GetViewTimeMs() const;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float delta_seconds, ELevelTick tick_type, FActorComponentTickFunction* this_tick_function) override;

private:
	// 새 상태가 이보다 오래 오지 않으면 입력을 멈춘다.
	static constexpr float STALE_SECONDS = 0.5f;
	// 받은 좌표와의 차이를 메우는 시간. 다음 좌표가 올 주기에 맞춘다.
	static constexpr float CORRECTION_SECONDS = 0.1f;
	static constexpr float ROTATION_SMOOTH_SECONDS = 0.05f;
	// 차이가 이보다 크면 보간하지 않고 바로 옮긴다.
	static constexpr float SNAP_DISTANCE = 100.0f;

	FRemoteMoveState state_;
	FVector2D move_input_ = FVector2D::ZeroVector;
	bool has_state_ = false;
	double state_time_ = 0.0;
	float view_pitch_ = 0.0f;
	// 남은 보간 이동량과 남은 시간
	FVector correction_ = FVector::ZeroVector;
	float correction_remaining_ = 0.0f;
};
