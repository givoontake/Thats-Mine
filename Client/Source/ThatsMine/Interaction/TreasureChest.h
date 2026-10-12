#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TreasureChest.generated.h"

class UAnimSequence;
class UNiagaraSystem;
class USkeletalMeshComponent;
class UStaticMeshComponent;

// 발견된 보물. 닫힌 금 상자로 나타나자마자 뚜껑이 열리며 금색 터짐 효과를 내고, 하늘로 금색 빛기둥이 선다. 놓는 위치는 보물 바닥 가운데다.
UCLASS()
class THATSMINE_API ATreasureChest : public AActor
{
	GENERATED_BODY()

public:
	ATreasureChest();

protected:
	virtual void BeginPlay() override;

private:
	// 100cm 원기둥을 기준으로 한 빛기둥 지름·높이 배율
	static constexpr float PILLAR_DIAMETER_SCALE = 1.2f;
	static constexpr float PILLAR_HEIGHT_SCALE = 40.0f;

	UPROPERTY(VisibleAnywhere, Category = "Treasure")
	TObjectPtr<USkeletalMeshComponent> chest_;

	UPROPERTY(VisibleAnywhere, Category = "Treasure")
	TObjectPtr<UStaticMeshComponent> light_pillar_;

	UPROPERTY()
	TObjectPtr<UAnimSequence> open_animation_;

	// 나이아가라 에셋은 게임 초기화 중 클래스 기본 객체를 만들 때 불러오면 쿠킹된 빌드가 종료되므로, 경로만 두고 나타날 때 불러온다.
	UPROPERTY()
	TSoftObjectPtr<UNiagaraSystem> open_effect_;
};
