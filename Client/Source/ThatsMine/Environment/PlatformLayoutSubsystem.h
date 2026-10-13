#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "platform_layout.h"
#include "PlatformLayoutSubsystem.generated.h"

// 게임 레벨에 이번 게임의 발판을 놓는다.
// 온라인은 서버가 게임 시작 때 정해 보낸 배치를 받아 놓고, 로컬 플레이는 같은 규칙으로 직접 정해 놓는다.
// 발판은 보이지 않고, 발판 길 힌트를 얻으면 내 화면에서만 그 길의 발판이 그 게임 동안 보인다.
UCLASS()
class THATSMINE_API UPlatformLayoutSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& world) override;
	virtual void Deinitialize() override;
	// 그 발판 길의 발판을 보이게 한다. 길 번호는 서버 발판 자리 표에서 길이 나오는 순서다.
	void RevealPath(int32 path);

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type world_type) const override;

private:
	void HandlePlatformLayoutPacket(const TArray<uint8>& payload);
	void SpawnPlatforms(const PlatformLayout& layout);

	bool is_active_ = false;

	// 놓은 발판과, 같은 순서로 그 발판이 속한 발판 길 번호
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> platforms_;
	TArray<int32> platform_paths_;
};
