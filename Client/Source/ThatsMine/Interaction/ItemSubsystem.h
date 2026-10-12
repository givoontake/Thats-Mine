#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Player/inventory_hint.h"
#include "ItemSubsystem.generated.h"

// 내가 연 상자의 아이템을 내 캐릭터에 넣고, 돋보기 탐색을 서버에 요청해 결과를 받는다.
// 탐색 결과는 그 등급의 돋보기를 하나 쓰게 하고, 배제 영역과 화면 위쪽 안내는 힌트 서브시스템에 넘긴다. 온라인 게임 레벨에서만 동작한다.
UCLASS()
class THATSMINE_API UItemSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& world) override;
	virtual void Deinitialize() override;
	// 그 등급의 돋보기로 탐색을 시작한다고 서버에 보낸다.
	void RequestSearch(EHintGrade grade) const;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type world_type) const override;

private:
	void HandleItemPacket(const TArray<uint8>& payload);
	void HandleSearchResultPacket(const TArray<uint8>& payload);

	bool is_active_ = false;
};
