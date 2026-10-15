#include "HintNoticeWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"
#include "UI/game_font.h"

TSharedRef<SWidget> UHintNoticeWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		UCanvasPanel* root = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = root;
		text_ = WidgetTree->ConstructWidget<UTextBlock>();
		// 밝은 하늘이나 바닥 위에서도 읽히게 진한 외곽선을 두른다.
		FSlateFontInfo font = GetGameFont(30);
		font.OutlineSettings.OutlineSize = 2;
		font.OutlineSettings.OutlineColor = FLinearColor(0.03f, 0.02f, 0.01f, 0.9f);
		text_->SetFont(font);
		text_->SetColorAndOpacity(FLinearColor(1.0f, 0.85f, 0.35f));
		text_->SetJustification(ETextJustify::Center);
		text_->SetRenderOpacity(0.0f);
		UCanvasPanelSlot* text_slot = root->AddChildToCanvas(text_);
		text_slot->SetAnchors(FAnchors(0.5f, 0.0f));
		text_slot->SetAlignment(FVector2D(0.5, 0.0));
		text_slot->SetPosition(FVector2D(0.0, 110.0));
		text_slot->SetAutoSize(true);
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	return Super::RebuildWidget();
}

void UHintNoticeWidget::ShowNotice(const FText& text)
{
	if (text_)
	{
		text_->SetText(text);
		elapsed_ = 0.0f;
	}
}

void UHintNoticeWidget::NativeTick(const FGeometry& geometry, float delta_time)
{
	Super::NativeTick(geometry, delta_time);
	if (elapsed_ < 0.0f)
	{
		return;
	}
	elapsed_ += delta_time;
	if (elapsed_ >= DISPLAY_SECONDS)
	{
		text_->SetRenderOpacity(0.0f);
		elapsed_ = -1.0f;
		return;
	}
	text_->SetRenderOpacity(FMath::Min(FMath::Clamp(elapsed_ / FADE_SECONDS, 0.0f, 1.0f), FMath::Clamp((DISPLAY_SECONDS - elapsed_) / FADE_SECONDS, 0.0f, 1.0f)));
}
