#include "SearchGaugeWidget.h"

#include "Engine/Texture2D.h"

USearchGaugeWidget::USearchGaugeWidget(const FObjectInitializer& object_initializer)
	: Super(object_initializer)
{
	fill_color_ = FLinearColor::FromSRGBColor(FColor(96, 196, 255, 225));
	icon_texture_ = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Thats_Mine/UI/Shared/T_MagnifierIcon.T_MagnifierIcon")));
}

void USearchGaugeWidget::Start(float duration_seconds)
{
	duration_ = FMath::Max(duration_seconds, 0.01f);
	elapsed_ = 0.0f;
	SetStunProgress(0.0f, false);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void USearchGaugeWidget::Stop()
{
	elapsed_ = -1.0f;
	SetVisibility(ESlateVisibility::Collapsed);
}

void USearchGaugeWidget::NativeTick(const FGeometry& geometry, float delta_time)
{
	// 채움은 따라가는 보간 없이 지난 시간 비율 그대로 보인다.
	if (elapsed_ >= 0.0f)
	{
		elapsed_ += delta_time;
		SetStunProgress(elapsed_ / duration_, false);
	}
	Super::NativeTick(geometry, delta_time);
}
