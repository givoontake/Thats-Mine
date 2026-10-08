#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Tower.generated.h"

class UStaticMeshComponent;

// 맵에 놓는 탑. 탑 메시 아래에 기초 메시를 붙여 비탈에서도 바닥이 뜨지 않게 한다. 액터 위치는 탑 바닥 가운데다.
UCLASS()
class THATSMINE_API ATower : public AActor
{
	GENERATED_BODY()

public:
	ATower();

private:
	UPROPERTY(VisibleAnywhere, Category = "Tower")
	TObjectPtr<UStaticMeshComponent> tower_mesh_;

	UPROPERTY(VisibleAnywhere, Category = "Tower")
	TObjectPtr<UStaticMeshComponent> foundation_mesh_;
};
