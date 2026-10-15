#include "PingMarkerWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CapsuleComponent.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/PingSubsystem.h"
#include "Styling/CoreStyle.h"
#include "UI/game_font.h"

TSharedRef<SWidget> UPingMarkerWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		UCanvasPanel* root = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = root;
		marker_canvas_ = WidgetTree->ConstructWidget<UCanvasPanel>();
		marker_canvas_->SetVisibility(ESlateVisibility::Collapsed);
		UCanvasPanelSlot* marker_slot = root->AddChildToCanvas(marker_canvas_);
		marker_slot->SetAlignment(FVector2D(0.5, 0.0));
		marker_slot->SetSize(FVector2D(120.0, ICON_SIZE + FONT_SIZE * 2.0));
		UImage* icon = WidgetTree->ConstructWidget<UImage>();
		icon->SetBrush(FSlateRoundedBoxBrush(FLinearColor(1.0f, 0.82f, 0.1f), ICON_SIZE * 0.5f, FLinearColor::White, 2.0f, FVector2f(ICON_SIZE)));
		UCanvasPanelSlot* icon_slot = marker_canvas_->AddChildToCanvas(icon);
		icon_slot->SetAnchors(FAnchors(0.5f, 0.0f));
		icon_slot->SetAlignment(FVector2D(0.5, 0.5));
		icon_slot->SetSize(FVector2D(ICON_SIZE));
		distance_text_ = WidgetTree->ConstructWidget<UTextBlock>();
		// 밝은 하늘이나 바닥 위에서도 읽히게 진한 외곽선을 두른다.
		FSlateFontInfo font = GetGameFont(FONT_SIZE);
		font.OutlineSettings.OutlineSize = 2;
		font.OutlineSettings.OutlineColor = FLinearColor(0.03f, 0.02f, 0.01f, 0.9f);
		distance_text_->SetFont(font);
		distance_text_->SetColorAndOpacity(FLinearColor(1.0f, 0.93f, 0.6f));
		UCanvasPanelSlot* text_slot = marker_canvas_->AddChildToCanvas(distance_text_);
		text_slot->SetAnchors(FAnchors(0.5f, 0.0f));
		text_slot->SetAlignment(FVector2D(0.5, 0.0));
		text_slot->SetPosition(FVector2D(0.0, ICON_SIZE * 0.5 + 2.0));
		text_slot->SetAutoSize(true);
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	return Super::RebuildWidget();
}

void UPingMarkerWidget::NativeTick(const FGeometry& geometry, float delta_time)
{
	Super::NativeTick(geometry, delta_time);
	const UPingSubsystem* pings = GetWorld() ? GetWorld()->GetSubsystem<UPingSubsystem>() : nullptr;
	APlayerController* controller = GetOwningPlayer();
	const ACharacter* character = Cast<ACharacter>(GetOwningPlayerPawn());
	if (!marker_canvas_ || !pings || !pings->GetPingLocation() || !controller || !character)
	{
		if (marker_canvas_)
		{
			marker_canvas_->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}
	marker_canvas_->SetVisibility(ESlateVisibility::HitTestInvisible);
	const FVector ping = *pings->GetPingLocation();
	const FVector feet = character->GetActorLocation() - FVector(0.0, 0.0, character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	distance_text_->SetText(FText::AsCultureInvariant(FString::Printf(TEXT("%.1f m"), FVector::Dist(ping, feet) / 100.0)));

	const FVector2D view_size = geometry.GetLocalSize();
	const FVector2D center = view_size * 0.5;
	const FVector2D half = center - FVector2D(SCREEN_MARGIN);
	FVector camera_location;
	FRotator camera_rotation;
	controller->GetPlayerViewPoint(camera_location, camera_rotation);
	const FVector local = camera_rotation.UnrotateVector(ping - camera_location);
	FVector2D position;
	const bool is_projected = local.X > 0.0 && UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(controller, ping, position, false);
	const FVector2D offset = position - center;
	if (!is_projected || FMath::Abs(offset.X) > half.X || FMath::Abs(offset.Y) > half.Y)
	{
		// 화면 밖이면 가운데에서 핑 쪽으로 뻗은 선이 가장자리와 만나는 곳에 붙인다. 카메라 뒤면 카메라 기준 좌우·위아래 방향을 쓴다.
		FVector2D direction = is_projected ? offset : FVector2D(local.Y, -local.Z);
		if (direction.IsNearlyZero())
		{
			direction = FVector2D(0.0, 1.0);
		}
		const double scale_x = FMath::IsNearlyZero(direction.X) ? DBL_MAX : half.X / FMath::Abs(direction.X);
		const double scale_y = FMath::IsNearlyZero(direction.Y) ? DBL_MAX : half.Y / FMath::Abs(direction.Y);
		position = center + direction * FMath::Min(scale_x, scale_y);
	}
	CastChecked<UCanvasPanelSlot>(marker_canvas_->Slot)->SetPosition(position);
}
