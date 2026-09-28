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

	UPROPERTY()
	TObjectPtr<UNiagaraSystem> open_effect_;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> animated_mesh_;

	FTimerHandle open_timer_;
	void FinishOpening();
};
