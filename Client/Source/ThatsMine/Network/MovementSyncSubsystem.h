#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Player/player_action.h"
#include "Player/remote_move_state.h"
#include "Player/weapon_type.h"
#include "MovementSyncSubsystem.generated.h"

class APlayerCharacter;

// 온라인 게임 중 내 캐릭터의 이동 상태와 동작을 서버에 보내고, 받은 다른 플레이어의 상태와 동작을 해당 캐릭터에 반영한다.
// 이동 상태는 주기마다 보내고, 받는 쪽 추측이 크게 어긋나는 변화가 생기면 주기보다 먼저 보낸다. 동작은 시작할 때 바로 보낸다.
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
	void SendMoveState();
	void HandleActionStarted(EPlayerAction action, EWeaponType weapon);
	void HandleMovePacket(const TArray<uint8>& payload);
	void HandleActionPacket(const TArray<uint8>& payload);
	void RemoveLeftPlayers();
	void HandleInvalidPacket();
	APlayerCharacter* FindRemotePlayer(int32 player_id) const;

	TMap<int32, TWeakObjectPtr<APlayerCharacter>> remote_players_;
	TWeakObjectPtr<APlayerCharacter> local_character_;
	FRemoteMoveState last_sent_state_;
	double last_send_time_ = 0.0;
	bool has_sent_state_ = false;
	bool is_active_ = false;
};
