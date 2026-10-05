#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "platform_layout.h"
#include "PlatformLayoutSubsystem.generated.h"

// 게임 레벨에 이번 게임의 발판을 놓는다.
// 온라인은 서버가 게임 시작 때 정해 보낸 배치를 받아 놓고, 로컬 플레이는 같은 규칙으로 직접 정해 놓는다.
UCLASS()
class THATSMINE_API UPlatformLayoutSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& world) override;
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type world_type) const override;

private:
	void HandlePlatformLayoutPacket(const TArray<uint8>& payload);
	void SpawnPlatforms(const PlatformLayout& layout);

	bool is_active_ = false;
};
