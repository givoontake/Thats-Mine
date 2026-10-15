#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMeshActor.h"
#include "HintBox.generated.h"

class UAnimSequence;
class UBoxComponent;
class UNiagaraSystem;
class USkeletalMesh;
class USkeletalMeshComponent;

UCLASS()
class THATSMINE_API AHintBox : public AStaticMeshActor
{

	GENERATED_BODY()

public:
	// 상호작용 조준 추적 채널(설정의 Interact). 상호작용 박스만 이 채널에 걸리는 물체로 따로 둔다.
	static constexpr ECollisionChannel INTERACT_CHANNEL = ECC_GameTraceChannel2;

	AHintBox();
	bool IsActivated() const { return is_activated_; }
	// 서버가 인정한 열림. 선물은 사라지고 나무 상자는 열리는 연출을 재생한다.
	void Open();
	// 이미 열린 나무 상자를 연출 없이 열린 모습으로 둔다.
	void ShowOpened();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type end_play_reason) override;

private:
	static constexpr float OPEN_DURATION = 0.5f;
	// 상호작용 박스를 메시 경계 상자보다 키우는 배율
	static constexpr float INTERACT_BOX_SCALE = 1.5f;

	// 상호작용 조준에만 걸리는 상자. 메시보다 커서 조준이 조금 빗나가도 잡힌다. 열리면 끈다.
	UPROPERTY(VisibleAnywhere, Category = "Hint Box")
	TObjectPtr<UBoxComponent> interact_box_;

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
