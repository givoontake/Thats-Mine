#include "CoordinateWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"
#include "Styling/CoreStyle.h"

TSharedRef<SWidget> UCoordinateWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		UCanvasPanel* root = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = root;
		coordinate_text_ = WidgetTree->ConstructWidget<UTextBlock>();
		// 남은 시간 표시와 같이 밝은 하늘이나 바닥 위에서도 읽히게 진한 외곽선을 두른다.
		FSlateFontInfo font = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), FONT_SIZE);
		font.OutlineSettings.OutlineSize = 2;
		font.OutlineSettings.OutlineColor = FLinearColor(0.03f, 0.02f, 0.01f, 0.9f);
		coordinate_text_->SetFont(font);
		coordinate_text_->SetColorAndOpacity(FLinearColor(1.0f, 0.94f, 0.79f));
		UCanvasPanelSlot* text_slot = root->AddChildToCanvas(coordinate_text_);
		text_slot->SetPosition(FVector2D(SCREEN_MARGIN, SCREEN_MARGIN));
		text_slot->SetAutoSize(true);
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	return Super::RebuildWidget();
}

void UCoordinateWidget::NativeTick(const FGeometry& geometry, float delta_time)
{
	Super::NativeTick(geometry, delta_time);
	const APawn* pawn = GetOwningPlayerPawn();
	if (!pawn || !coordinate_text_)
	{
		return;
	}
	const FVector location = pawn->GetActorLocation();
	const FIntVector decimeters(FMath::RoundToInt32(location.X / 10.0), FMath::RoundToInt32(location.Y / 10.0), FMath::RoundToInt32(location.Z / 10.0));
	if (decimeters == shown_decimeters_)
	{
		return;
	}
	shown_decimeters_ = decimeters;
	coordinate_text_->SetText(FText::AsCultureInvariant(FString::Printf(TEXT("X %.1f  Y %.1f  Z %.1f m"), decimeters.X / 10.0, decimeters.Y / 10.0, decimeters.Z / 10.0)));
}
