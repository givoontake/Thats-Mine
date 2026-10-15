#include "MinimapWidget.h"
#include "MapViewWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "GameFramework/Pawn.h"
#include "settings.h"

TSharedRef<SWidget> UMinimapWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		UCanvasPanel* root = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = root;
		UImage* frame = WidgetTree->ConstructWidget<UImage>();
		frame->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.022f, 0.016f, 0.010f, 0.85f), 4.0f, FLinearColor(0.18f, 0.12f, 0.075f), 2.0f));
		UCanvasPanelSlot* frame_slot = root->AddChildToCanvas(frame);
		frame_slot->SetSize(FVector2D(FRAME_SIZE, FRAME_SIZE));
		map_view_ = WidgetTree->ConstructWidget<UMapViewWidget>();
		map_view_->SetMarkerScale(MARKER_SCALE);
		UCanvasPanelSlot* view_slot = root->AddChildToCanvas(map_view_);
		view_slot->SetPosition(FVector2D(BORDER, BORDER));
		view_slot->SetSize(FVector2D(VIEW_PIXELS, VIEW_PIXELS));
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	return Super::RebuildWidget();
}

// 내 위치가 창 가운데에 오도록 지도의 보이는 범위를 옮긴다.
void UMinimapWidget::NativeTick(const FGeometry& geometry, float delta_time)
{
	Super::NativeTick(geometry, delta_time);
	const APawn* pawn = GetOwningPlayerPawn();
	if (!pawn || !map_view_)
	{
		return;
	}
	const FVector2D view_size(VIEW_SIZE / (MAP_MAX[1] - MAP_MIN[1]), VIEW_SIZE / (MAP_MAX[0] - MAP_MIN[0]));
	map_view_->SetView(UMapViewWidget::GetMapPosition(pawn->GetActorLocation()) - view_size * 0.5, view_size);
}
