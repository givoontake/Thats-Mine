#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HintDropSubsystem.generated.h"

class AStaticMeshActor;

// 기절한 플레이어가 떨어뜨린 힌트를 두루마리로 바닥에 놓고, F키 줍기를 서버에 요청해 서버가 인정한 줍기만 보여 준다.
// 서버는 떨어뜨린 플레이어의 캡슐 중심을 보내고, 공중에서 떨어뜨렸을 수 있어 그 아래 바닥을 찾아 놓는다. 온라인 게임 레벨에서만 동작한다.
UCLASS()
class THATSMINE_API UHintDropSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& world) override;
	virtual void Deinitialize() override;
	// 캡슐 중심에서 가장 가까운 두루마리를 주우려고 서버에 요청한다. 서버와 같은 거리·높이 조건에 맞는 두루마리가 없으면 보내지 않고 false를 돌려준다.
	bool RequestPickup(const FVector& capsule_location) const;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type world_type) const override;

private:
	void HandleHintDropPacket(const TArray<uint8>& payload);
	void HandleHintPickupPacket(const TArray<uint8>& payload);

	// 드랍 번호마다 바닥에 놓은 두루마리와 서버가 보낸 위치. 주우면 둘 다 뺀다.
	UPROPERTY()
	TMap<uint16, TObjectPtr<AStaticMeshActor>> scrolls_;
	TMap<uint16, FVector> drop_locations_;

	bool is_active_ = false;
};
