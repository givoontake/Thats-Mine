#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Player/player_action.h"
#include "Player/remote_move_state.h"
#include "Player/shot_result.h"
#include "Player/weapon_type.h"
#include "MovementSyncSubsystem.generated.h"

class APlayerCharacter;

// 온라인 게임 중 내 캐릭터의 이동 상태와 동작을 서버에 보내고, 받은 다른 플레이어의 상태와 동작을 해당 캐릭터에 반영한다.
// 이동 상태는 주기마다 보내고, 받는 쪽 추측이 크게 어긋나는 변화가 생기면 주기보다 먼저 보낸다. 동작은 시작할 때 바로 보낸다.
// 총을 쏘면 조준 방향과 카메라 위치를 서버에 보내고, 서버가 판정한 명중과 기절 게이지·기절을 받는다.
// 서버가 이동을 인정하지 않으면 받은 위치로 내 캐릭터를 바로 옮기고 확인을 보낸다.
UCLASS()
class THATSMINE_API UMovementSyncSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& world) override;
	virtual void Deinitialize() override;
	virtual void Tick(float delta_seconds) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;
	// 다른 플레이어 캐릭터를 조종자 없이 만든다. 이미 있으면 그 캐릭터를 돌려준다.
	APlayerCharacter* SpawnRemotePlayer(int32 player_id, const FTransform& transform);

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type world_type) const override;

private:
	// is_forced면 보내는 간격과 관계없이 지금 위치를 바로 보낸다.
	void SendMoveState(bool is_forced);
	void SendInputChanges(const APlayerCharacter& character);
	void HandleActionStarted(EPlayerAction action, EWeaponType weapon);
	UFUNCTION()
	void HandleShotFired(const FShotResult& shot);
	void HandleHitPacket(const TArray<uint8>& payload);
	void HandleStunGaugePacket(const TArray<uint8>& payload);
	void HandleAmmoUsePacket(const TArray<uint8>& payload);
	void HandleAmmoGainPacket(const TArray<uint8>& payload);
	void HandleReloadDonePacket(const TArray<uint8>& payload);
	void HandleTimeSyncPacket(const TArray<uint8>& payload);
	void HandlePickaxeChargesPacket(const TArray<uint8>& payload);
	void HandleStunApplyPacket(const TArray<uint8>& payload);
	void HandleStunReleasePacket(const TArray<uint8>& payload);
	void HandleMoveCorrectionPacket(const TArray<uint8>& payload);
	void HandleMovePacket(const TArray<uint8>& payload);
	void HandleMoveInputPacket(const TArray<uint8>& payload);
	void HandleAimPacket(const TArray<uint8>& payload);
	void HandleEquipPacket(const TArray<uint8>& payload);
	void HandleActionPacket(const TArray<uint8>& payload);
	void HandleReloadPacket(const TArray<uint8>& payload);
	void RemoveLeftPlayers();
	void HandleInvalidPacket();
	APlayerCharacter* FindRemotePlayer(int32 player_id) const;

	TMap<int32, TWeakObjectPtr<APlayerCharacter>> remote_players_;
	TWeakObjectPtr<APlayerCharacter> local_character_;
	FRemoteMoveState last_sent_state_;
	double last_send_time_ = 0.0;
	bool has_sent_state_ = false;
	// 마지막으로 보낸 이동 키 입력·조준 여부·장비. 처음에는 has_sent_input_이 거짓이라 모두 보낸다.
	int8 sent_input_right_ = 0;
	int8 sent_input_forward_ = 0;
	bool is_sent_aiming_ = false;
	EWeaponType sent_weapon_ = EWeaponType::PICKAXE;
	bool has_sent_input_ = false;
	bool is_active_ = false;
	// 받은 서버 시각과 받은 순간의 실제 시각(FPlatformTime 초). 발사의 동작 시각은 이 서버 시각에 그 뒤 흐른 실제 시간을 더한다.
	uint32 sync_server_time_ms_ = 0;
	double sync_real_time_ = 0.0;
};
