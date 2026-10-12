#include "HintPopupWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"
#include "UI/Hint/HintGradeBadge.h"

TSharedRef<SWidget> UHintPopupWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		UCanvasPanel* root = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = root;
		// 탄 표시(오른쪽 아래에서 32 떨어진 높이 88의 패널) 바로 위에 오른쪽 끝을 맞춘다.
		UVerticalBox* box = WidgetTree->ConstructWidget<UVerticalBox>();
		UCanvasPanelSlot* box_slot = root->AddChildToCanvas(box);
		box_slot->SetAnchors(FAnchors(1.0f, 1.0f));
		box_slot->SetAlignment(FVector2D(1.0, 1.0));
		box_slot->SetPosition(FVector2D(-32.0, -136.0));
		box_slot->SetAutoSize(true);

		title_ = WidgetTree->ConstructWidget<UTextBlock>();
		title_->SetText(NSLOCTEXT("Hint", "NewHint", "새 힌트 획득!"));
		// 밝은 바닥 위에서도 읽히게 진한 외곽선을 두른다.
		FSlateFontInfo title_font = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 24);
		title_font.OutlineSettings.OutlineSize = 2;
		title_font.OutlineSettings.OutlineColor = FLinearColor(0.03f, 0.02f, 0.01f, 0.9f);
		title_->SetFont(title_font);
		title_->SetColorAndOpacity(FLinearColor(1.0f, 0.64f, 0.17f));
		UVerticalBoxSlot* title_slot = box->AddChildToVerticalBox(title_);
		title_slot->SetHorizontalAlignment(HAlign_Right);
		title_slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));

		// 문장이 길면 패널 폭 안에서 줄을 바꾼다. 패널 모양은 인벤토리의 힌트 줄과 같다.
		USizeBox* panel_size = WidgetTree->ConstructWidget<USizeBox>();
		panel_size->SetWidthOverride(480.0f);
		box->AddChildToVerticalBox(panel_size)->SetHorizontalAlignment(HAlign_Right);
		panel_ = WidgetTree->ConstructWidget<UBorder>();
		panel_->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.04f, 0.03f, 0.02f, 0.85f), 4.0f, FLinearColor(0.18f, 0.12f, 0.075f), 1.0f));
		panel_->SetPadding(FMargin(14.0f, 8.0f));
		panel_size->AddChild(panel_);
		content_ = WidgetTree->ConstructWidget<UHorizontalBox>();
		panel_->AddChild(content_);

		// 패널은 투명도로 숨기지 않고 화면 밖에 비켜 두어 숨긴다.
		title_->SetRenderOpacity(0.0f);
		panel_->SetRenderTranslation(FVector2D(SLIDE_DISTANCE, 0.0));
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	return Super::RebuildWidget();
}

void UHintPopupWidget::Enqueue(const FInventoryHint& hint)
{
	queue_.Add(hint);
	if (elapsed_ < 0.0f)
	{
		ShowNext();
	}
}

void UHintPopupWidget::ShowNext()
{
	if (queue_.IsEmpty() || !content_)
	{
		elapsed_ = -1.0f;
		return;
	}
	const FInventoryHint hint = queue_[0];
	queue_.RemoveAt(0);
	content_->ClearChildren();
	UHintGradeBadge* badge = WidgetTree->ConstructWidget<UHintGradeBadge>();
	badge->SetGrade(hint.grade);
	content_->AddChildToHorizontalBox(badge)->SetVerticalAlignment(VAlign_Center);
	UTextBlock* text = WidgetTree->ConstructWidget<UTextBlock>();
	text->SetText(hint.text);
	text->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 22));
	text->SetColorAndOpacity(FLinearColor(0.96f, 0.90f, 0.78f));
	text->SetAutoWrapText(true);
	UHorizontalBoxSlot* text_slot = content_->AddChildToHorizontalBox(text);
	text_slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	text_slot->SetPadding(FMargin(22.0f, 5.0f, 8.0f, 5.0f));
	text_slot->SetVerticalAlignment(VAlign_Center);
	elapsed_ = 0.0f;
	layout_wait_frames_ = LAYOUT_WAIT_FRAMES;
}

void UHintPopupWidget::NativeTick(const FGeometry& geometry, float delta_time)
{
	Super::NativeTick(geometry, delta_time);
	if (elapsed_ < 0.0f)
	{
		return;
	}
	if (layout_wait_frames_ > 0)
	{
		--layout_wait_frames_;
		return;
	}
	elapsed_ += delta_time;
	if (elapsed_ >= DISPLAY_SECONDS)
	{
		title_->SetRenderOpacity(0.0f);
		panel_->SetRenderTranslation(FVector2D(SLIDE_DISTANCE, 0.0));
		ShowNext();
		return;
	}
	// 들어올 때는 빨리 출발해 천천히 멈추고, 나갈 때는 천천히 출발해 빨리 빠져나간다.
	const float slide_in = FMath::Clamp(elapsed_ / SLIDE_SECONDS, 0.0f, 1.0f);
	const float slide_out = FMath::Clamp((DISPLAY_SECONDS - elapsed_) / SLIDE_SECONDS, 0.0f, 1.0f);
	const float offset = 1.0f - FMath::Min(1.0f - FMath::Pow(1.0f - slide_in, 3.0f), 1.0f - FMath::Pow(1.0f - slide_out, 3.0f));
	const float fade_out = FMath::Clamp((DISPLAY_SECONDS - elapsed_) / FADE_OUT_SECONDS, 0.0f, 1.0f);
	title_->SetRenderOpacity(FMath::Min(FMath::Clamp(elapsed_ / TITLE_FADE_IN_SECONDS, 0.0f, 1.0f), fade_out));
	panel_->SetRenderTranslation(FVector2D(SLIDE_DISTANCE * offset, 0.0));
}
