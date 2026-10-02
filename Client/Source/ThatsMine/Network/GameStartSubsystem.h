#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameStartSubsystem.generated.h"

struct FStreamableHandle;
class UMatchTimerWidget;

// 온라인 게임 레벨이 열린 뒤 게임에 필요한 준비를 마치고 서버에 로딩 완료를 알린다.
// 서버가 보낸 스폰 패킷으로 내 캐릭터와 다른 플레이어 캐릭터를 만들고, 플레이 시작 패킷을 받으면 플레이 상태로 바꾼다.
// 맵이 보인 뒤에는 게임 시작 전까지 화면 상단에 대기 안내와 남은 시간을 보여 준다.
UCLASS()
class THATSMINE_API UGameStartSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& world) override;
	virtual void Deinitialize() override;
	virtual void Tick(float delta_seconds) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type world_type) const override;

private:
	void SendLoadComplete();
	void HandleSpawnPacket(const TArray<uint8>& payload);
	void HandlePlayStartPacket(const TArray<uint8>& payload);
	void HandleGameReadyPacket(const TArray<uint8>& payload);
	void HandleGameStartPacket(const TArray<uint8>& payload);
	UMatchTimerWidget* ShowTimerWidget();
	void HandleInvalidPacket();

	// 처음 쓸 때 불러오는 에셋을 게임 중에 불러오지 않도록 미리 불러 두고, 게임 동안 붙잡아 둔다.
	TSharedPtr<FStreamableHandle> preload_handle_;
	UPROPERTY(Transient)
	TObjectPtr<UMatchTimerWidget> timer_widget_;
	double prepare_start_time_ = 0.0;
	bool is_active_ = false;
	bool has_sent_load_complete_ = false;
	// 시작 대기를 거쳤는지. 게임이 시작된 뒤 합류하면 대기 없이 바로 게임 시작을 받는다.
	bool has_received_game_ready_ = false;
};
