#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "StunGaugeWidget.generated.h"

class UTexture2D;

UCLASS()
class THATSMINE_API UStunGaugeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 화면에 붙일 때 쓰는 위젯 크기
	static constexpr float WIDGET_WIDTH = 440.0f;
	static constexpr float WIDGET_HEIGHT = 44.0f;

	// 누적 비율만 전달받는다. 스턴 발생이나 수치 증감은 이 위젯에서 처리하지 않는다.
	UFUNCTION(BlueprintCallable, Category = "Stun Gauge")
	void SetStunProgress(float progress, bool animate = true);

	// 기절이 끝난 뒤 보호 중이면 남은 초를 받아 아이콘을 방패로 바꾸고 아래에 남은 초를 표시한다. 0이면 기절 아이콘으로 돌아간다.
	void SetProtectionSeconds(int32 seconds);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& geometry, float delta_time) override;
	virtual int32 NativePaint(const FPaintArgs& args, const FGeometry& geometry, const FSlateRect& culling_rect, FSlateWindowElementList& draw_elements, int32 layer_id, const FWidgetStyle& widget_style, bool is_parent_enabled) const override;

	UPROPERTY(EditDefaultsOnly, Category = "Stun Gauge")
	FLinearColor fill_color_ = FLinearColor::FromSRGBColor(FColor(255, 138, 36, 225));

	UPROPERTY(EditDefaultsOnly, Category = "Stun Gauge")
	TSoftObjectPtr<UTexture2D> icon_texture_ = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Thats_Mine/UI/Shared/T_StunIcon.T_StunIcon")));

	UPROPERTY(EditDefaultsOnly, Category = "Stun Gauge")
	TSoftObjectPtr<UTexture2D> protection_icon_texture_ = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Thats_Mine/UI/Shared/T_ShieldIcon.T_ShieldIcon")));

private:
	UPROPERTY(Transient)
	FSlateBrush icon_brush_;

	UPROPERTY(Transient)
	FSlateBrush protection_icon_brush_;

	float progress_ = 0.0f;
	float displayed_progress_ = 0.0f;
	int32 protection_seconds_ = 0;
};
