#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TowerLayoutSubsystem.generated.h"

// 게임 레벨에 이번 게임의 탑을 놓는다.
// 온라인은 서버가 게임 시작 때 후보 자리에서 골라 보낸 탑 위치를 받아 놓는다. 로컬 플레이는 후보 데이터가 서버에만 있어 놓지 않는다.
UCLASS()
class THATSMINE_API UTowerLayoutSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& world) override;
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type world_type) const override;

private:
	void HandleTowerLayoutPacket(const TArray<uint8>& payload);

	bool is_active_ = false;
};
