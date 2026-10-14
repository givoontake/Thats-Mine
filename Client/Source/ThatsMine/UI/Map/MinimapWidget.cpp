#include "MinimapWidget.h"
#include "MapGridWidget.h"
#include "MapPlayerMarkerWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Pawn.h"
#include "Interaction/HintSubsystem.h"
#include "Interaction/PingSubsystem.h"
#include "Network/GameStartSubsystem.h"
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

		// 지도는 창 밖으로 넘치게 크게 놓고 창 경계에서 잘라 보인다.
		view_canvas_ = WidgetTree->ConstructWidget<UCanvasPanel>();
		view_canvas_->SetClipping(EWidgetClipping::ClipToBoundsAlways);
		UCanvasPanelSlot* view_slot = root->AddChildToCanvas(view_canvas_);
		view_slot->SetPosition(FVector2D(BORDER, BORDER));
		view_slot->SetSize(FVector2D(VIEW_PIXELS, VIEW_PIXELS));
		map_image_ = WidgetTree->ConstructWidget<UImage>();
		map_image_->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr, TEXT("/Game/Thats_Mine/UI/Map/T_WorldMap.T_WorldMap")));
		view_canvas_->AddChildToCanvas(map_image_);
		exclusion_image_ = WidgetTree->ConstructWidget<UImage>();
		exclusion_image_->SetColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, EXCLUSION_SHADE_OPACITY));
		exclusion_image_->SetVisibility(ESlateVisibility::Collapsed);
		view_canvas_->AddChildToCanvas(exclusion_image_);
		grid_ = WidgetTree->ConstructWidget<UMapGridWidget>();
		view_canvas_->AddChildToCanvas(grid_);
		treasure_marker_ = WidgetTree->ConstructWidget<UImage>();
		treasure_marker_->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr, TEXT("/Game/Thats_Mine/UI/Map/T_TreasureMarker.T_TreasureMarker")));
		treasure_marker_->SetVisibility(ESlateVisibility::Collapsed);
		UCanvasPanelSlot* treasure_slot = view_canvas_->AddChildToCanvas(treasure_marker_);
		treasure_slot->SetAlignment(FVector2D(0.5, 0.5));
		treasure_slot->SetSize(FVector2D(TREASURE_MARKER_SIZE));
		ping_marker_ = WidgetTree->ConstructWidget<UImage>();
		ping_marker_->SetBrush(FSlateRoundedBoxBrush(FLinearColor(1.0f, 0.82f, 0.1f), PING_MARKER_SIZE * 0.5f, FLinearColor::White, 1.5f, FVector2f(PING_MARKER_SIZE)));
		ping_marker_->SetVisibility(ESlateVisibility::Collapsed);
		UCanvasPanelSlot* ping_slot = view_canvas_->AddChildToCanvas(ping_marker_);
		ping_slot->SetAlignment(FVector2D(0.5, 0.5));
		ping_slot->SetSize(FVector2D(PING_MARKER_SIZE));
		player_marker_ = WidgetTree->ConstructWidget<UMapPlayerMarkerWidget>();
		player_marker_->SetRenderTransformPivot(FVector2D(0.5, 0.5));
		player_marker_->SetRenderScale(FVector2D(MARKER_SCALE, MARKER_SCALE));
		UCanvasPanelSlot* marker_slot = view_canvas_->AddChildToCanvas(player_marker_);
		marker_slot->SetAlignment(FVector2D(0.5, 0.5));
		marker_slot->SetPosition(FVector2D(VIEW_PIXELS * 0.5, VIEW_PIXELS * 0.5));
		marker_slot->SetSize(FVector2D(40.0, 40.0));
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	return Super::RebuildWidget();
}

void UMinimapWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UHintSubsystem* hints = GetWorld()->GetSubsystem<UHintSubsystem>())
	{
		hints->on_exclusion_mask_changed_.AddUObject(this, &UMinimapWidget::UpdateExclusionTexture);
	}
	UpdateExclusionTexture();
}

void UMinimapWidget::NativeDestruct()
{
	if (UWorld* world = GetWorld())
	{
		if (UHintSubsystem* hints = world->GetSubsystem<UHintSubsystem>())
		{
			hints->on_exclusion_mask_changed_.RemoveAll(this);
		}
	}
	Super::NativeDestruct();
}

void UMinimapWidget::UpdateExclusionTexture()
{
	const UHintSubsystem* hints = GetWorld() ? GetWorld()->GetSubsystem<UHintSubsystem>() : nullptr;
	UTexture2D* texture = hints ? hints->GetExclusionTexture() : nullptr;
	if (!exclusion_image_ || !texture)
	{
		return;
	}
	exclusion_image_->SetBrushFromTexture(texture);
	exclusion_image_->SetVisibility(ESlateVisibility::HitTestInvisible);
}

// 내 위치가 창 가운데에 오도록 지도·배제·격자·표시를 옮긴다. 지도 사진은 위가 x가 작은 쪽, 왼쪽이 y가 큰 쪽이다.
void UMinimapWidget::NativeTick(const FGeometry& geometry, float delta_time)
{
	Super::NativeTick(geometry, delta_time);
	const APawn* pawn = GetOwningPlayerPawn();
	if (!pawn || !map_image_)
	{
		return;
	}
	const FVector location = pawn->GetActorLocation();
	const FVector2D world_size(MAP_MAX[1] - MAP_MIN[1], MAP_MAX[0] - MAP_MIN[0]);
	const FVector2D position((MAP_MAX[1] - location.Y) / world_size.X, (location.X - MAP_MIN[0]) / world_size.Y);
	const FVector2D map_pixels = world_size * (VIEW_PIXELS / VIEW_SIZE);
	const FVector2D map_origin = FVector2D(VIEW_PIXELS * 0.5) - position * map_pixels;
	for (UWidget* widget : { static_cast<UWidget*>(map_image_.Get()), static_cast<UWidget*>(exclusion_image_.Get()), static_cast<UWidget*>(grid_.Get()) })
	{
		UCanvasPanelSlot* slot = CastChecked<UCanvasPanelSlot>(widget->Slot);
		slot->SetPosition(map_origin);
		slot->SetSize(map_pixels);
	}
	const UGameStartSubsystem* game_start = GetWorld()->GetSubsystem<UGameStartSubsystem>();
	PlaceMarker(treasure_marker_, game_start ? game_start->GetRevealedTreasureLocation() : TOptional<FVector>(), map_origin, map_pixels);
	const UPingSubsystem* pings = GetWorld()->GetSubsystem<UPingSubsystem>();
	PlaceMarker(ping_marker_, pings ? pings->GetPingLocation() : TOptional<FVector>(), map_origin, map_pixels);
	// 큰 지도의 화살표와 같은 각도로 돌린다.
	const FVector forward = pawn->GetActorForwardVector();
	player_marker_->SetRenderTransformAngle(FMath::RadiansToDegrees(FMath::Atan2(-forward.Y, -forward.X)));
}

void UMinimapWidget::PlaceMarker(UImage* marker, const TOptional<FVector>& location, const FVector2D& map_origin, const FVector2D& map_pixels)
{
	if (!location)
	{
		marker->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	marker->SetVisibility(ESlateVisibility::HitTestInvisible);
	const FVector2D position((MAP_MAX[1] - location->Y) / (MAP_MAX[1] - MAP_MIN[1]), (location->X - MAP_MIN[0]) / (MAP_MAX[0] - MAP_MIN[0]));
	CastChecked<UCanvasPanelSlot>(marker->Slot)->SetPosition(map_origin + position * map_pixels);
}
