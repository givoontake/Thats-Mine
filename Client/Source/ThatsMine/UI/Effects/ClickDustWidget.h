#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "click_dust_particle.h"
#include "ClickDustWidget.generated.h"

class UTexture2D;

UCLASS()
class THATSMINE_API UClickDustWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION()
	void PlayAtCursor();
	void ResetEffects();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& geometry, float delta_time) override;

	UPROPERTY(EditDefaultsOnly, Category = "Click Effect")
	TSoftObjectPtr<UTexture2D> texture_ = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Thats_Mine/UI/Shared/T_ClickDust.T_ClickDust")));

	UPROPERTY(EditDefaultsOnly, Category = "Click Effect", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float duration_ = 0.35f;

private:
	static constexpr int32 MAX_PARTICLES = 12;
	static constexpr float PUFF_SIZE = 64.0f;
	UPROPERTY(Transient)
	TArray<FClickDustParticle> particles_;
	int32 next_particle_ = 0;
};
