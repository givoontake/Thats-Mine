#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Player/local_move_step.h"
#include "Player/player_action.h"
#include "Player/remote_move_state.h"
#include "Player/shot_result.h"
#include "Player/weapon_type.h"
#include "MovementSyncSubsystem.generated.h"

class APlayerCharacter;

// 온라인 게임 중 내 캐릭터의 이동과 동작을 서버에 보내고, 받은 다른 플레이어의 상태와 동작을 해당 캐릭터에 반영한다.
// 게임 패킷(조준·장비·동작·재장전·발사·상자 열기·블록 놓기)은 만든 게임 프레임의 전송 프레임 번호를 넣어 들고 있다가,
// 그 번호가 끝나 이동 컴포넌트가 그 번호를 계산하면 그 번호의 패킷 뒤에 이동 패킷을 붙여, 그 게임 프레임이 끝날 때 한 번에 보낸다.
// 총을 쏘면 퍼진 탄으로 찾은 대상과 그 대상이 화면에서 향하던 틱 번호를 함께 보내고, 서버가 판정한 명중과 기절 게이지·기절을 받는다.
// 서버가 이동을 인정하지 않으면 받은 상태를 지금 이동 상태로 세팅하고, 들고 있던 패킷을 버린 뒤 확인을 보낸다.
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
	// 내 캐릭터 스폰을 받은 순간 번호 시계를 0번에서 시작하고, 쌓인 패킷(스폰 알림)을 보낸다.
	void StartFrameClock();
	// 게임 패킷을 이번 게임 프레임의 전송 프레임 번호로 들고 있는다. payload는 헤더 뒤 내용이고, 맨 앞 frame_number 칸에 번호를 쓴다.
	// 번호 시계가 시작하지 않았으면 버린다.
	void AppendFramePacket(uint8 type, TArray<uint8> payload);

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type world_type) const override;

private:
	// 조종하는 캐릭터가 바뀌었으면 알림을 다시 묶는다.
	void BindLocalCharacter();
	void HandleWeaponSelected(EWeaponType weapon);
	void HandleAimToggled(bool is_aiming);
	// 계산한 번호의 들고 있던 패킷과 이동 패킷을 보낼 패킷 뒤에 이어 쓴다.
	void HandleMoveStep(const FLocalMoveStep& step);
	void HandleActionStarted(EPlayerAction action, EWeaponType weapon);
	UFUNCTION()
	void HandleShotFired(const FShotResult& shot);
	void HandleHitPacket(const TArray<uint8>& payload);
	void HandleStunGaugePacket(const TArray<uint8>& payload);
	void HandleAmmoUsePacket(const TArray<uint8>& payload);
	void HandleAmmoGainPacket(const TArray<uint8>& payload);
	void HandleReloadDonePacket(const TArray<uint8>& payload);
	void HandlePickaxeChargesPacket(const TArray<uint8>& payload);
	void HandleStunApplyPacket(const TArray<uint8>& payload);
	void HandleStunReleasePacket(const TArray<uint8>& payload);
	void HandleMoveRollbackPacket(const TArray<uint8>& payload);
	void HandleMovePacket(const TArray<uint8>& payload);
	void HandleMoveStopPacket(const TArray<uint8>& payload);
	void HandleAimPacket(const TArray<uint8>& payload);
	void HandleEquipPacket(const TArray<uint8>& payload);
	void HandleActionPacket(const TArray<uint8>& payload);
	void HandleReloadPacket(const TArray<uint8>& payload);
	void RemoveLeftPlayers();
	void HandleInvalidPacket();
	APlayerCharacter* FindRemotePlayer(int32 player_id) const;
	// 다른 플레이어의 이동 상태·멈춤·롤백에 붙어 온 방 틱 번호를 받았다.
	void MarkTickNumber(uint32 tick_number);

	// 번호가 끝나기를 기다리는 게임 패킷. 만든 순서대로 둔다.
	struct FFramePacket
	{
		uint32 frame_number = 0;
		uint8 type = 0;
		TArray<uint8> payload;
	};

	TMap<int32, TWeakObjectPtr<APlayerCharacter>> remote_players_;
	TWeakObjectPtr<APlayerCharacter> local_character_;
	TArray<FFramePacket> frame_packets_;
	// 이번 게임 프레임에 보낼 이동 패킷을 붙였는지. 게임 프레임이 끝날 때 쌓인 것을 한 번에 보낸다.
	bool has_move_packet_ = false;
	bool is_active_ = false;
	// 받은 가장 최근 방 틱 번호. 빗나간 발사의 되감을 시점으로 보낸다.
	uint32 latest_tick_number_ = 0;
};
