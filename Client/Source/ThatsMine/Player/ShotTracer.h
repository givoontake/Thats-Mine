#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShotTracer.generated.h"

class UMaterialInstanceDynamic;
class UStaticMeshComponent;

// 총 발사 궤적. 총구에서 탄이 끝난 곳까지 얇은 원기둥을 놓고, 가운데는 흰색이고 가장자리로 갈수록 노랑·주황으로 빛나게 그린다.
// LIFE_SECONDS 동안 흐려지며 사라진다. 다른 플레이어의 발사만 그린다. 내 화면은 카메라와 총구 위치가 달라 궤적이 어색하다.
UCLASS()
class THATSMINE_API AShotTracer : public AActor
{
	GENERATED_BODY()

public:
	AShotTracer();
	// 무기 메시 소켓(총구)에서 end까지 원기둥을 놓고, 사라질 때까지 총구를 따라 시작점을 옮긴다.
	// 발사 동작이 궤적보다 늦게 시작해도 궤적이 총구에서 나오게 한다.
	void FollowMuzzle(USceneComponent* weapon_mesh, FName socket, const FVector& end);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float delta_time) override;
	// 시작점과 끝점 사이에 원기둥을 놓는다.
	void SetSegment(const FVector& start, const FVector& end);

private:
	static constexpr float LIFE_SECONDS = 0.2f;
	// 원기둥 지름(cm)
	static constexpr double THICKNESS = 2.5;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> mesh_;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> material_;

	TWeakObjectPtr<USceneComponent> weapon_mesh_;
	FName socket_;
	FVector end_ = FVector::ZeroVector;

	float age_ = 0.0f;
};
