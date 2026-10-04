#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DamageDirectionWidget.generated.h"

UCLASS()
class THATSMINE_API UDamageDirectionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// attack_direction은 피격자에서 공격이 온 쪽을 향하는 월드 방향이다.
	UFUNCTION(BlueprintCallable, Category = "Damage Indicator")
	void ShowHit(const FVector& attack_direction, float camera_yaw);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& geometry, float delta_time) override;
	virtual int32 NativePaint(const FPaintArgs& args, const FGeometry& geometry, const FSlateRect& culling_rect, FSlateWindowElementList& draw_elements, int32 layer_id, const FWidgetStyle& widget_style, bool is_parent_enabled) const override;

	UPROPERTY(EditDefaultsOnly, Category = "Damage Indicator", meta = (ClampMin = "1.0"))
	float radius_ = 92.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Damage Indicator", meta = (ClampMin = "1.0", ClampMax = "180.0"))
	float arc_degrees_ = 80.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Damage Indicator", meta = (ClampMin = "1.0"))
	float thickness_ = 12.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Damage Indicator", meta = (ClampMin = "0.1"))
	float duration_ = 0.65f;

	UPROPERTY(EditDefaultsOnly, Category = "Damage Indicator")
	FLinearColor color_ = FLinearColor::FromSRGBColor(FColor(255, 138, 36, 140));

private:
	float angle_ = 0.0f;
	float elapsed_ = 0.0f;
};
