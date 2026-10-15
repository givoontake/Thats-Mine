#include "GameTimeWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Styling/CoreStyle.h"
#include "UI/Map/MinimapWidget.h"
#include "UI/game_font.h"

TSharedRef<SWidget> UGameTimeWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		UCanvasPanel* root = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = root;
		time_text_ = WidgetTree->ConstructWidget<UTextBlock>();
		// 밝은 하늘이나 바닥 위에서도 읽히게 진한 외곽선을 두른다.
		FSlateFontInfo font = GetGameFont(FONT_SIZE);
		font.OutlineSettings.OutlineSize = 2;
		font.OutlineSettings.OutlineColor = FLinearColor(0.03f, 0.02f, 0.01f, 0.9f);
		time_text_->SetFont(font);
		time_text_->SetColorAndOpacity(FLinearColor(1.0f, 0.94f, 0.79f));
		time_text_->SetJustification(ETextJustify::Right);
		UCanvasPanelSlot* text_slot = root->AddChildToCanvas(time_text_);
		text_slot->SetAnchors(FAnchors(1.0f, 0.0f));
		text_slot->SetAlignment(FVector2D(1.0, 0.0));
		// 오른쪽 위 미니맵 바로 아래에 둔다.
		text_slot->SetPosition(FVector2D(-SCREEN_MARGIN, SCREEN_MARGIN + UMinimapWidget::FRAME_SIZE + MINIMAP_GAP));
		text_slot->SetAutoSize(true);
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	return Super::RebuildWidget();
}

void UGameTimeWidget::StartCountdown(float seconds)
{
	end_time_ = GetWorld()->GetTimeSeconds() + FMath::Max(0.0f, seconds);
	is_counting_ = true;
	shown_seconds_ = INDEX_NONE;
	UpdateTimeText();
}

void UGameTimeWidget::StopCountdown()
{
	is_counting_ = false;
}

void UGameTimeWidget::NativeTick(const FGeometry& geometry, float delta_time)
{
	Super::NativeTick(geometry, delta_time);
	if (is_counting_)
	{
		UpdateTimeText();
	}
}

// 남은 시간은 올림해서 초 단위로 보여 주어 0:00:00은 시간이 다 되었을 때만 나온다.
void UGameTimeWidget::UpdateTimeText()
{
	if (!time_text_ || !GetWorld())
	{
		return;
	}
	const int32 seconds = FMath::CeilToInt32(FMath::Max(0.0, end_time_ - GetWorld()->GetTimeSeconds()));
	if (seconds == shown_seconds_)
	{
		return;
	}
	shown_seconds_ = seconds;
	time_text_->SetText(FText::Format(NSLOCTEXT("GameTime", "Remaining", "남은 시간 {0}"), FText::AsCultureInvariant(FString::Printf(TEXT("%d:%02d:%02d"), seconds / 3600, seconds / 60 % 60, seconds % 60))));
}
