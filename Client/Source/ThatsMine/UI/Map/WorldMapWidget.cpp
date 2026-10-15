#include "WorldMapWidget.h"
#include "MapViewWidget.h"
#include "Engine/GameInstance.h"
#include "Settings/KeyBindingSubsystem.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "InputCoreTypes.h"

UWorldMapWidget::UWorldMapWidget(const FObjectInitializer& object_initializer)
	: Super(object_initializer)
{
	SetIsFocusable(true);
	SetVisibility(ESlateVisibility::Visible);
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
		map_view_ = WidgetTree->ConstructWidget<UMapViewWidget>();
		map_view_->SetPlacesVisible(true);
		map_canvas_->AddChildToCanvas(map_view_)->SetSize(FVector2D(MAP_SIZE, MAP_SIZE));
	}
	return Super::RebuildWidget();
}

// 지도 창은 한 번 만들어 다시 쓰므로, 닫았다 열어도 확대·이동 상태를 그대로 보인다.
void UWorldMapWidget::NativeConstruct()
{
	Super::NativeConstruct();
	UpdateMap();
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
	// 확대한 지도에서 창(MAP_SIZE)에 들어오는 부분을 지도 정규 좌표로 보인다.
	map_view_->SetView(-pan_ / (MAP_SIZE * zoom_), FVector2D(1.0 / zoom_, 1.0 / zoom_));
}

void UWorldMapWidget::Close()
{
	on_close_requested_.ExecuteIfBound();
}

FReply UWorldMapWidget::NativeOnPreviewKeyDown(const FGeometry& geometry, const FKeyEvent& event)
{
	const UKeyBindingSubsystem* bindings = GetGameInstance()->GetSubsystem<UKeyBindingSubsystem>();
	if (event.GetKey() == bindings->GetKey(EKeyAction::INVENTORY) || event.GetKey() == bindings->GetKey(EKeyAction::HELP))
	{
		return FReply::Handled();
	}
	if (event.GetKey() == bindings->GetKey(EKeyAction::MAP) || event.GetKey() == EKeys::Escape)
	{
		if (!event.IsRepeat()) { Close(); }
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(geometry, event);
}

FReply UWorldMapWidget::NativeOnKeyDown(const FGeometry& geometry, const FKeyEvent& event)
{
	// 이동과 점프는 게임 입력으로 전달하고, 나머지 동작은 지도에서 소비한다.
	const UKeyBindingSubsystem* bindings = GetGameInstance()->GetSubsystem<UKeyBindingSubsystem>();
	for (const EKeyAction action : { EKeyAction::MOVE_FORWARD, EKeyAction::MOVE_BACKWARD, EKeyAction::MOVE_LEFT, EKeyAction::MOVE_RIGHT, EKeyAction::JUMP })
	{
		if (event.GetKey() == bindings->GetKey(action))
		{
			return FReply::Unhandled();
		}
	}
	return FReply::Handled();
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
