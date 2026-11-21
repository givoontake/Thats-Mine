#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "remote_move_state.h"
#include "RemoteMovementComponent.generated.h"

// 다른 플레이어 캐릭터에 붙어, 서버가 틱마다 보낸 이동 상태로 그 캐릭터를 옮긴다.
// 새 상태를 받으면 지금 그린 위치·방향에서 받은 위치·방향까지 MOVE_STEP_MS 동안 옮기고, 다음 상태를 받으면 그때 그린 위치에서 다시 옮긴다.
// 이동은 계산하지 않고, 받은 속도와 공중 여부는 애니메이션에 쓰도록 이동 컴포넌트에 넣어 둔다.
UCLASS()
class THATSMINE_API URemoteMovementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URemoteMovementComponent();

	// 서버가 보낸 방 틱 tick_number의 이동 상태
	void ApplyState(const FRemoteMoveState& state, uint32 tick_number);
	// 서버가 멈춘 것으로 판단한 위치로 옮기고 수평 속도를 0으로 둔다.
	void ApplyMoveStop(const FVector& location, uint32 tick_number);
	// 서버가 롤백한 이동 상태로 옮기는 시간 없이 바로 바꾼다.
	void ApplyRollback(const FRemoteMoveState& state, uint32 tick_number);
	void SetAiming(bool is_aiming);
	void SetWeapon(EWeaponType weapon);
	float GetViewPitch() const { return view_pitch_; }
	// 지금 향하는 상태의 방 틱 번호와, 앞 위치에서 그 상태까지 간 정도(0~100). 발사할 때 이 시점으로 서버가 되감는다.
	uint32 GetTickNumber() const { return tick_number_; }
	uint8 GetTickProgress() const;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float delta_seconds, ELevelTick tick_type, FActorComponentTickFunction* this_tick_function) override;

private:
	// 향하는 상태와 그 방 틱 번호
	FRemoteMoveState state_;
	uint32 tick_number_ = 0;
	bool has_state_ = false;
	// 옮기기 시작한 위치·방향과 그 뒤 흐른 시간(초)
	FVector start_location_ = FVector::ZeroVector;
	float start_yaw_ = 0.0f;
	float start_view_pitch_ = 0.0f;
	float move_elapsed_ = 0.0f;
	float view_pitch_ = 0.0f;

	// 지금 그린 위치·방향에서 state로 옮기기 시작한다. 처음 받은 상태면 바로 옮긴다.
	void StartMove(const FRemoteMoveState& state, uint32 tick_number);
	// 받은 속도와 공중 여부를 이동 컴포넌트에 넣는다.
	void ApplyMovementMode(const FRemoteMoveState& state);
};
