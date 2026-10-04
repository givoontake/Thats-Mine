#include "LocalPlayWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"

TSharedRef<SWidget> ULocalPlayWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		UCanvasPanel* canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = canvas;
		UTextBlock* notice = WidgetTree->ConstructWidget<UTextBlock>();
		notice->SetText(NSLOCTEXT("LocalSession", "PlayNotice", "로컬 테스트  |  F10: 종료"));
		notice->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 18));
		notice->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.8f, 0.4f)));
		notice->SetShadowColorAndOpacity(FLinearColor::Black);
		notice->SetShadowOffset(FVector2D(1.0, 1.0));
		UCanvasPanelSlot* slot = canvas->AddChildToCanvas(notice);
		slot->SetPosition(FVector2D(24.0, 24.0));
		slot->SetAutoSize(true);
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	return Super::RebuildWidget();
}
