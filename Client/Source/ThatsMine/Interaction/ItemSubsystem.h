#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Player/inventory_hint.h"
#include "ItemSubsystem.generated.h"

// 내가 연 상자의 아이템을 내 캐릭터에 넣는다. 온라인 게임 레벨에서만 동작한다.
UCLASS()
class THATSMINE_API UItemSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& world) override;
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type world_type) const override;

private:
	void HandleItemPacket(const TArray<uint8>& payload);

	bool is_active_ = false;
};
