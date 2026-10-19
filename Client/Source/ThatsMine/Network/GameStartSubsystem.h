#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameStartSubsystem.generated.h"

struct FStreamableHandle;
class UGameTimeWidget;
class UMatchTimerWidget;

DECLARE_MULTICAST_DELEGATE(FOnTreasureFound);

// 온라인 게임 레벨이 열린 뒤 게임에 필요한 준비를 마치고 서버에 로딩 완료를 알린다.
// 서버가 보낸 스폰 패킷으로 내 캐릭터와 다른 플레이어 캐릭터를 만들고, 플레이 시작 패킷을 받으면 플레이 상태로 바꾼다.
// 맵이 보인 뒤에는 게임 시작 전까지 화면 상단에 대기 안내와 남은 시간을 보여 준다. 누군가 보물을 발견하면 같은 자리에 발견 안내를 보여 준다.
// 게임이 시작되면 화면 오른쪽 위에 게임 제한시간까지 남은 시간을 보여 주고, 아무도 찾지 못한 채 시간이 끝나면 보물 위치를 공개한다.
// 시험용으로 보물 위치를 요청할 수 있고, 서버는 방장의 요청에만 답해 플레이 중인 전원에게 알린다.
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
	// 누군가 발견했거나 시간이 끝나 공개된 보물 바닥 가운데 위치. 그 전에는 비어 있다.
	const TOptional<FVector>& GetTreasureLocation() const { return treasure_location_; }
	// 시험용 요청으로 받은 보물 바닥 가운데 위치. 받기 전에는 비어 있다.
	const TOptional<FVector>& GetRevealedTreasureLocation() const { return revealed_treasure_location_; }
	// 시험용으로 서버에 보물 위치를 요청한다.
	void RequestTreasureReveal() const;

	FOnTreasureFound on_treasure_found_;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type world_type) const override;

private:
	void SendLoadComplete();
	void HandleSpawnPacket(const TArray<uint8>& payload);
	void HandlePlayStartPacket(const TArray<uint8>& payload);
	void HandleGameReadyPacket(const TArray<uint8>& payload);
	void HandleGameStartPacket(const TArray<uint8>& payload);
	void HandleTreasureFoundPacket(const TArray<uint8>& payload);
	void HandleTimeOverPacket(const TArray<uint8>& payload);
	void HandleTreasureLocationPacket(const TArray<uint8>& payload);
	void HandleTreasureRevealPacket(const TArray<uint8>& payload);
	// 보물 자리에 보물 상자를 놓고 지도가 보물 위치를 표시하도록 알린다. 남은 시간 표시는 멈춘다.
	void RevealTreasure(const float location[3], float yaw);
	UMatchTimerWidget* ShowTimerWidget();
	void HandleInvalidPacket();

	// 처음 쓸 때 불러오는 에셋을 게임 중에 불러오지 않도록 미리 불러 두고, 게임 동안 붙잡아 둔다.
	TSharedPtr<FStreamableHandle> preload_handle_;
	UPROPERTY(Transient)
	TObjectPtr<UMatchTimerWidget> timer_widget_;
	UPROPERTY(Transient)
	TObjectPtr<UGameTimeWidget> game_time_widget_;
	double prepare_start_time_ = 0.0;
	bool is_active_ = false;
	bool has_sent_load_complete_ = false;
	// 시작 대기를 거쳤는지. 게임이 시작된 뒤 합류하면 대기 없이 바로 게임 시작을 받는다.
	bool has_received_game_ready_ = false;
	TOptional<FVector> treasure_location_;
	TOptional<FVector> revealed_treasure_location_;
};
