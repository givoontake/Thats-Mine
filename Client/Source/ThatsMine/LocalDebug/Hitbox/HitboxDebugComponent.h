#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HitboxDebugComponent.generated.h"

class UPhysicsAsset;
class USkeletalMeshComponent;

// 히트박스 도형을 뼈 위치에 맞춰 매 프레임 테두리로 그린다.
UCLASS()
class THATSMINE_API UHitboxDebugComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHitboxDebugComponent();
	virtual void TickComponent(float delta_time, ELevelTick tick_type, FActorComponentTickFunction* this_tick_function) override;

	void SetHitbox(USkeletalMeshComponent* mesh, UPhysicsAsset* hitbox_asset);

private:
	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> mesh_;

	UPROPERTY(Transient)
	TObjectPtr<UPhysicsAsset> hitbox_asset_;
};
