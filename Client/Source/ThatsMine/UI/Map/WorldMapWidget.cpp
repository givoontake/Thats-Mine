#include "WorldMapWidget.h"
#include "MapPlayerMarkerWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"

UWorldMapWidget::UWorldMapWidget(const FObjectInitializer& object_initializer)
	: Super(object_initializer)
{
	SetIsFocusable(true);
	SetVisibility(ESlateVisibility::Visible);
	places_ = {
		{FText::AsCultureInvariant(TEXT("Dragon Ridge")), FVector2D(0.350, 0.151), 28},
		{FText::AsCultureInvariant(TEXT("Start Dome")), FVector2D(0.841, 0.070), 14},
		{FText::AsCultureInvariant(TEXT("Ring Ruins")), FVector2D(0.225, 0.269), 20},
		{FText::AsCultureInvariant(TEXT("Oak Ridge")), FVector2D(0.188, 0.416), 25},
		{FText::AsCultureInvariant(TEXT("Bamboo\nGrove")), FVector2D(0.560, 0.309), 20},
		{FText::AsCultureInvariant(TEXT("Pyramid")), FVector2D(0.632, 0.404), 18},
		{FText::AsCultureInvariant(TEXT("Egyptian\nVillage")), FVector2D(0.236, 0.540), 18},
		{FText::AsCultureInvariant(TEXT("Asian\nVillage")), FVector2D(0.547, 0.552), 21},
		{FText::AsCultureInvariant(TEXT("Maple Peak")), FVector2D(0.779, 0.566), 21},
		{FText::AsCultureInvariant(TEXT("Star Peak")), FVector2D(0.251, 0.756), 23},
		{FText::AsCultureInvariant(TEXT("Ship Peak")), FVector2D(0.773, 0.818), 23},
		{FText::AsCultureInvariant(TEXT("Oak Lake")), FVector2D(0.409, 0.332), 23},
		{FText::AsCultureInvariant(TEXT("Mini Lake")), FVector2D(0.668, 0.318), 18},
		{FText::AsCultureInvariant(TEXT("Blue Lake")), FVector2D(0.608, 0.730), 17},
		{FText::AsCultureInvariant(TEXT("Pond")), FVector2D(0.817, 0.031), 14}
	};
}

TSharedRef<SWidget> UWorldMapWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		UCanvasPanel* root = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = root;
		map_canvas_ = WidgetTree->ConstructWidget<UCanvasPanel>();
		map_canvas_->SetVisibility(ESlateVisibility::Visible);
		map_canvas_->SetClipping(EWidgetClipping::ClipToBoundsAlways);
		UCanvasPanelSlot* map_slot = root->AddChildToCanvas(map_canvas_);
		map_slot->SetAnchors(FAnchors(0.5f, 0.5f));
		map_slot->SetAlignment(FVector2D(0.5, 0.5));
		map_slot->SetPosition(FVector2D::ZeroVector);
		map_slot->SetSize(FVector2D(MAP_SIZE, MAP_SIZE));
		map_image_ = WidgetTree->ConstructWidget<UImage>();
		map_image_->SetBrushFromTexture(map_texture_.LoadSynchronous());
		map_image_->SetVisibility(ESlateVisibility::HitTestInvisible);
		map_canvas_->AddChildToCanvas(map_image_)->SetSize(FVector2D(MAP_SIZE, MAP_SIZE));
		for (const FMapPlace& place : places_)
		{
			UTextBlock* label = WidgetTree->ConstructWidget<UTextBlock>();
			label->SetText(place.name);
			// 두 줄 지명도 가운데 정렬한다.
			label->SetJustification(ETextJustify::Center);
			label->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), place.font_size * MAX_ZOOM));
			label->SetColorAndOpacity(FLinearColor(0.035f, 0.02f, 0.012f, 0.65f));
			label->SetShadowColorAndOpacity(FLinearColor(1.0f, 0.96f, 0.83f, 0.38f));
			label->SetShadowOffset(FVector2D(0.0, MAX_ZOOM));
			label->SetRenderTransformPivot(FVector2D(0.5, 0.5));
			label->SetVisibility(ESlateVisibility::HitTestInvisible);
			UCanvasPanelSlot* label_slot = map_canvas_->AddChildToCanvas(label);
			label_slot->SetAlignment(FVector2D(0.5, 0.5));
			label_slot->SetAutoSize(true);
			place_labels_.Add(label);
		}

		player_marker_ = WidgetTree->ConstructWidget<UMapPlayerMarkerWidget>();
		player_marker_->SetVisibility(ESlateVisibility::HitTestInvisible);
		player_marker_->SetRenderTransformPivot(FVector2D(0.5, 0.5));
		UCanvasPanelSlot* marker_slot = map_canvas_->AddChildToCanvas(player_marker_);
		marker_slot->SetAlignment(FVector2D(0.5, 0.5));
		marker_slot->SetSize(FVector2D(40.0, 40.0));
		marker_slot->SetZOrder(10);

	}
	return Super::RebuildWidget();
}

void UWorldMapWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ResetView();
}

void UWorldMapWidget::NativeDestruct()
{
	is_dragging_ = false;
	Super::NativeDestruct();
}

void UWorldMapWidget::NativeTick(const FGeometry& geometry, float delta_time)
{
	Super::NativeTick(geometry, delta_time);
	if (zoom_ != target_zoom_)
	{
		// 프레임 속도와 관계없이 목표 배율로 수렴하고, 커서 아래 지형을 기준으로 확대한다.
		double next_zoom = target_zoom_ + (zoom_ - target_zoom_) * FMath::Exp(-ZOOM_SPEED * delta_time);
		if (FMath::IsNearlyEqual(next_zoom, target_zoom_, 0.0001))
		{
			next_zoom = target_zoom_;
		}
		pan_ = zoom_anchor_ - (zoom_anchor_ - pan_) * (next_zoom / zoom_);
		zoom_ = next_zoom;
		UpdateMap();
	}
	else
	{
		UpdatePlayerMarker();
	}
}

void UWorldMapWidget::UpdatePlayerMarker()
{
	const APawn* pawn = GetOwningPlayerPawn();
	if (!pawn)
	{
		player_marker_->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	player_marker_->SetVisibility(ESlateVisibility::HitTestInvisible);
	const FVector location = pawn->GetActorLocation();
	const FVector2D world_size = world_max_ - world_min_;
	const FVector2D position((world_max_.Y - location.Y) / world_size.Y, (location.X - world_min_.X) / world_size.X);
	const FVector2D map_position = position * MAP_SIZE * zoom_ + pan_;
	UCanvasPanelSlot* marker_slot = CastChecked<UCanvasPanelSlot>(player_marker_->Slot);
	if (!marker_slot->GetPosition().Equals(map_position, 0.01))
	{
		marker_slot->SetPosition(map_position);
	}
	const FVector forward = pawn->GetActorForwardVector();
	const float angle = FMath::RadiansToDegrees(FMath::Atan2(-forward.Y, -forward.X));
	if (!FMath::IsNearlyEqual(player_marker_->GetRenderTransform().Angle, angle, 0.01f))
	{
		player_marker_->SetRenderTransformAngle(angle);
	}
}

void UWorldMapWidget::ChangeZoom(float wheel_delta, const FVector2D& anchor)
{
	zoom_anchor_ = anchor;
	target_zoom_ = FMath::Clamp(target_zoom_ * FMath::Pow(WHEEL_ZOOM_FACTOR, static_cast<double>(wheel_delta)), 1.0, MAX_ZOOM);
}

void UWorldMapWidget::UpdateMap()
{
	const double minimum = MAP_SIZE * (1.0 - zoom_);
	pan_.X = FMath::Clamp(pan_.X, minimum, 0.0);
	pan_.Y = FMath::Clamp(pan_.Y, minimum, 0.0);
	UCanvasPanelSlot* image_slot = CastChecked<UCanvasPanelSlot>(map_image_->Slot);
	image_slot->SetPosition(pan_);
	image_slot->SetSize(FVector2D(MAP_SIZE * zoom_, MAP_SIZE * zoom_));
	for (int32 index = 0; index < place_labels_.Num(); ++index)
	{
		CastChecked<UCanvasPanelSlot>(place_labels_[index]->Slot)->SetPosition(places_[index].position * MAP_SIZE * zoom_ + pan_);
		// 최대 크기의 글자를 연속 배율로 그려 정수 글꼴 크기 변경에 따른 튐을 없앤다.
		place_labels_[index]->SetRenderScale(FVector2D(zoom_ / MAX_ZOOM, zoom_ / MAX_ZOOM));
	}
	UpdatePlayerMarker();
}

void UWorldMapWidget::ResetView()
{
	zoom_ = 1.0;
	target_zoom_ = 1.0;
	zoom_anchor_ = FVector2D::ZeroVector;
	pan_ = FVector2D::ZeroVector;
	UpdateMap();
}

void UWorldMapWidget::Close()
{
	on_close_requested_.ExecuteIfBound();
}

FReply UWorldMapWidget::NativeOnPreviewKeyDown(const FGeometry& geometry, const FKeyEvent& event)
{
	if (event.GetKey() == EKeys::Tab)
	{
		return FReply::Handled();
	}
	if (event.GetKey() == EKeys::M || event.GetKey() == EKeys::Escape)
	{
		if (!event.IsRepeat()) { Close(); }
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(geometry, event);
}

FReply UWorldMapWidget::NativeOnKeyDown(const FGeometry& geometry, const FKeyEvent& event)
{
	const FKey key = event.GetKey();
	// 이동과 점프는 게임 입력으로 전달하고, 나머지 동작은 지도에서 소비한다.
	return key == EKeys::W || key == EKeys::A || key == EKeys::S || key == EKeys::D || key == EKeys::SpaceBar ? FReply::Unhandled() : FReply::Handled();
}

FReply UWorldMapWidget::NativeOnMouseWheel(const FGeometry& geometry, const FPointerEvent& event)
{
	const FGeometry& map_geometry = map_canvas_->GetCachedGeometry();
	if (map_geometry.IsUnderLocation(event.GetScreenSpacePosition()) && !FMath::IsNearlyZero(event.GetWheelDelta()))
	{
		ChangeZoom(event.GetWheelDelta(), map_geometry.AbsoluteToLocal(event.GetScreenSpacePosition()));
	}
	return FReply::Handled();
}

FReply UWorldMapWidget::NativeOnMouseButtonDown(const FGeometry& geometry, const FPointerEvent& event)
{
	const FGeometry& map_geometry = map_canvas_->GetCachedGeometry();
	if (event.GetEffectingButton() == EKeys::LeftMouseButton && map_geometry.IsUnderLocation(event.GetScreenSpacePosition()))
	{
		is_dragging_ = true;
		drag_position_ = map_geometry.AbsoluteToLocal(event.GetScreenSpacePosition());
		return FReply::Handled().CaptureMouse(TakeWidget()).SetUserFocus(TakeWidget());
	}
	return FReply::Handled();
}

FReply UWorldMapWidget::NativeOnMouseButtonUp(const FGeometry& geometry, const FPointerEvent& event)
{
	if (event.GetEffectingButton() == EKeys::LeftMouseButton && is_dragging_)
	{
		is_dragging_ = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Handled();
}

FReply UWorldMapWidget::NativeOnMouseMove(const FGeometry& geometry, const FPointerEvent& event)
{
	if (is_dragging_)
	{
		const FVector2D position = map_canvas_->GetCachedGeometry().AbsoluteToLocal(event.GetScreenSpacePosition());
		const FVector2D delta = position - drag_position_;
		pan_ += delta;
		zoom_anchor_ += delta;
		drag_position_ = position;
		UpdateMap();
	}
	return FReply::Handled();
}

void UWorldMapWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& event)
{
	is_dragging_ = false;
	Super::NativeOnMouseCaptureLost(event);
}
