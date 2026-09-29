#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMeshActor.h"
#include "HintBox.generated.h"

class UAnimSequence;
class UNiagaraSystem;
class USkeletalMesh;
class USkeletalMeshComponent;

UCLASS()
class THATSMINE_API AHintBox : public AStaticMeshActor
{

	GENERATED_BODY()

public:
	AHintBox();
	void Activate(bool is_shot);

protected:
	virtual void EndPlay(const EEndPlayReason::Type end_play_reason) override;

private:
	static constexpr float OPEN_DURATION = 0.5f;

	UPROPERTY(VisibleInstanceOnly, Category = "Hint Box")
	bool is_activated_ = false;

	UPROPERTY()
	TObjectPtr<USkeletalMesh> chest_mesh_;

	UPROPERTY()
	TObjectPtr<UAnimSequence> open_animation_;

	UPROPERTY()
	TObjectPtr<UStaticMesh> open_mesh_;

	// 나이아가라 에셋은 게임 초기화 중 클래스 기본 객체를 만들 때 불러오면 쿠킹된 빌드가 종료되므로, 경로만 두고 처음 열 때 불러온다.
	UPROPERTY()
	TSoftObjectPtr<UNiagaraSystem> open_effect_;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> animated_mesh_;

	FTimerHandle open_timer_;
	void FinishOpening();
};
