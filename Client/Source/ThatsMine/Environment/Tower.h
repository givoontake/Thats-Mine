#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "game_packets.h"
#include "Tower.generated.h"

class UStaticMesh;
class UStaticMeshComponent;

// 맵에 놓는 탑. 탑 메시 아래에 기초 메시를 붙여 비탈에서도 바닥이 뜨지 않게 한다. 액터 위치는 탑 바닥 가운데다.
UCLASS()
class THATSMINE_API ATower : public AActor
{
	GENERATED_BODY()

public:
	ATower();

	// 탑 메시를 종류에 맞게 바꾼다. 컴포넌트 등록 전에 바꿔야 하므로 지연 생성 중 FinishSpawning 전에 부른다.
	void SetKind(TowerKind kind);

private:
	// TowerKind 순서의 탑 메시
	UPROPERTY()
	TObjectPtr<UStaticMesh> tower_meshes_[TOWER_KIND_COUNT];

	UPROPERTY(VisibleAnywhere, Category = "Tower")
	TObjectPtr<UStaticMeshComponent> tower_mesh_;

	UPROPERTY(VisibleAnywhere, Category = "Tower")
	TObjectPtr<UStaticMeshComponent> foundation_mesh_;
};
