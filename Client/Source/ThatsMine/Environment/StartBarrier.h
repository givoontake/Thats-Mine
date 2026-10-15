#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StartBarrier.generated.h"

class UStaticMeshComponent;

// 시작 구역(피라미드 안 방) 입구를 막는 반투명 벽. 게임 시작 전에는 캐릭터 이동만 막고, 게임이 시작되면 바로 사라진다.
// 레벨에 입구마다 하나씩 두고 크기는 배율로 맞춘다. 로컬 플레이는 서버의 게임 시작이 없으므로 처음부터 숨긴다.
UCLASS()
class THATSMINE_API AStartBarrier : public AActor
{
	GENERATED_BODY()

public:
	AStartBarrier();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Start Barrier")
	void ShowBarrier();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Start Barrier")
	void HideBarrier();

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Start Barrier")
	TObjectPtr<UStaticMeshComponent> barrier_mesh_;
};
