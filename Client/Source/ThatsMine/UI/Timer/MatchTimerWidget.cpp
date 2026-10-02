#include "MatchTimerWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "Styling/CoreStyle.h"

// 시간이 넘어갈 때 커졌다가 원래 크기로 돌아오는 정도와 시간
static constexpr float PULSE_SCALE = 0.15f;
static constexpr float PULSE_SECONDS = 0.25f;

TSharedRef<SWidget> UMatchTimerWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		UCanvasPanel* canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MatchTimerCanvas"));
		WidgetTree->RootWidget = canvas;
		content_ = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MatchTimerContent"));
		UCanvasPanelSlot* content_slot = canvas->AddChildToCanvas(content_);
		content_slot->SetAnchors(FAnchors(0.5f, 0.0f));
		content_slot->SetAlignment(FVector2D(0.5, 0.0));
		content_slot->SetPosition(FVector2D(0.0, 32.0));
		content_slot->SetAutoSize(true);

		// 안내 글씨를 위에, 남은 초를 그 아래에 둔다.
		message_text_ = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MatchTimerMessage"));
		message_text_->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 20));
		message_text_->SetColorAndOpacity(FSlateColor(text_color_));
		message_text_->SetShadowOffset(FVector2D(1.5, 1.5));
		message_text_->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.75f));
		message_text_->SetJustification(ETextJustify::Center);
		UVerticalBoxSlot* message_slot = content_->AddChildToVerticalBox(message_text_);
		message_slot->SetHorizontalAlignment(HAlign_Center);

		// 배경 없이 숫자만 보이므로, 밝은 배경에서도 읽히게 진한 갈색 외곽선과 그림자를 넣는다.
		timer_text_ = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MatchTimerText"));
		FSlateFontInfo timer_font = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 36);
		timer_font.OutlineSettings.OutlineSize = 2;
		timer_font.OutlineSettings.OutlineColor = FLinearColor::FromSRGBColor(FColor(58, 36, 24, 255));
		timer_text_->SetFont(timer_font);
		timer_text_->SetShadowOffset(FVector2D(2.0, 2.0));
		timer_text_->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f));
		timer_text_->SetJustification(ETextJustify::Center);
		timer_text_->SetRenderTransformPivot(FVector2D(0.5, 0.5));
		UVerticalBoxSlot* timer_slot = content_->AddChildToVerticalBox(timer_text_);
		timer_slot->SetHorizontalAlignment(HAlign_Center);
		timer_slot->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 0.0f));

		timer_text_->SetVisibility(ESlateVisibility::Collapsed);
		message_text_->SetVisibility(ESlateVisibility::Collapsed);
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	return Super::RebuildWidget();
}

void UMatchTimerWidget::StartCountdown(float seconds)
{
	end_time_ = GetWorld()->GetTimeSeconds() + FMath::Max(0.0f, seconds);
	is_counting_ = true;
	shown_seconds_ = INDEX_NONE;
	if (timer_text_)
	{
		timer_text_->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	UpdateTimerText();
}

void UMatchTimerWidget::HideTimer()
{
	is_counting_ = false;
	if (timer_text_)
	{
		timer_text_->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UMatchTimerWidget::SetMessage(const FText& message)
{
	if (message_text_)
	{
		message_text_->SetText(message);
		message_text_->SetVisibility(message.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
}

void UMatchTimerWidget::NativeTick(const FGeometry& geometry, float delta_time)
{
	Super::NativeTick(geometry, delta_time);
	if (is_counting_)
	{
		UpdateTimerText();
	}
}

void UMatchTimerWidget::UpdateTimerText()
{
	if (!timer_text_ || !GetWorld())
	{
		return;
	}
	// 남은 시간은 올림해서 초 단위 숫자로만 보여 주어 0은 시간이 다 되었을 때만 나온다.
	const double remaining = FMath::Max(0.0, end_time_ - GetWorld()->GetTimeSeconds());
	const int32 seconds = FMath::CeilToInt(remaining);
	const bool is_warning = seconds <= warning_seconds_;
	if (seconds != shown_seconds_)
	{
		shown_seconds_ = seconds;
		timer_text_->SetText(FText::AsNumber(seconds));
		timer_text_->SetColorAndOpacity(FSlateColor(is_warning ? warning_color_ : text_color_));
	}
	// 강조 구간에서는 숫자가 바뀐 직후 잠깐 커졌다가 돌아온다.
	const float since_change = static_cast<float>(seconds - remaining);
	const float pulse = is_warning && seconds > 0 ? PULSE_SCALE * FMath::Max(0.0f, 1.0f - since_change / PULSE_SECONDS) : 0.0f;
	timer_text_->SetRenderScale(FVector2D(1.0f + pulse));
}
