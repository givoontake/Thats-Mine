#include "EquipmentWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Styling/CoreStyle.h"
#include "UI/game_font.h"

TSharedRef<SWidget> UEquipmentWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		UCanvasPanel* canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("EquipmentCanvas"));
		WidgetTree->RootWidget = canvas;
		crosshair_ = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Crosshair"));
		UCanvasPanelSlot* crosshair_slot = canvas->AddChildToCanvas(crosshair_);
		crosshair_slot->SetAnchors(FAnchors(0.5f, 0.5f));
		crosshair_slot->SetAlignment(FVector2D(0.5, 0.5));
		crosshair_slot->SetPosition(FVector2D::ZeroVector);
		crosshair_slot->SetSize(FVector2D(24.0, 24.0));
		// 중앙은 비우고 작은 점을 넣어 카메라 명중 기준점을 가리지 않는다.
		const FVector4 segments[] = {FVector4(1.0, 11.0, 6.0, 2.0), FVector4(17.0, 11.0, 6.0, 2.0), FVector4(11.0, 1.0, 2.0, 6.0), FVector4(11.0, 17.0, 2.0, 6.0), FVector4(11.0, 11.0, 2.0, 2.0)};
		for (int32 layer = 0; layer < 2; ++layer)
		{
			const double outline = layer == 0 ? 1.0 : 0.0;
			for (const FVector4& segment : segments)
			{
				UImage* line = WidgetTree->ConstructWidget<UImage>();
				line->SetBrush(FSlateColorBrush(layer == 0 ? FLinearColor(0.0f, 0.0f, 0.0f, 0.85f) : FLinearColor::White));
				UCanvasPanelSlot* line_slot = crosshair_->AddChildToCanvas(line);
				line_slot->SetPosition(FVector2D(segment.X - outline, segment.Y - outline));
				line_slot->SetSize(FVector2D(segment.Z + outline * 2.0, segment.W + outline * 2.0));
				line_slot->SetZOrder(layer);
			}
		}

		UCanvasPanel* panel = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("EquipmentPanel"));
		UCanvasPanelSlot* panel_slot = canvas->AddChildToCanvas(panel);
		panel_slot->SetAnchors(FAnchors(1.0f, 1.0f));
		panel_slot->SetAlignment(FVector2D(1.0, 1.0));
		panel_slot->SetPosition(FVector2D(-32.0, -32.0));
		panel_slot->SetSize(FVector2D(400.0, 88.0));

		name_text_ = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EquipmentName"));
		name_text_->SetFont(GetGameFont(24));
		name_text_->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.94f, 0.79f)));
		UCanvasPanelSlot* name_slot = panel->AddChildToCanvas(name_text_);
		name_slot->SetAutoSize(true);

		divider_ = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("EquipmentDivider"));
		divider_->SetBrushColor(FLinearColor(0.27f, 0.29f, 0.30f, 0.8f));
		UCanvasPanelSlot* divider_slot = panel->AddChildToCanvas(divider_);
		divider_slot->SetAnchors(FAnchors(DIVIDER_ANCHOR, 0.5f));
		divider_slot->SetAlignment(FVector2D(0.5, 0.5));
		divider_slot->SetSize(FVector2D(1.0, 40.0));
		divider_slot->SetPosition(FVector2D::ZeroVector);

		ammo_text_ = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EquipmentAmmo"));
		ammo_text_->SetFont(GetGameFont(36));
		UCanvasPanelSlot* ammo_slot = panel->AddChildToCanvas(ammo_text_);
		ammo_slot->SetAnchors(FAnchors(DIVIDER_ANCHOR, 0.5f));
		ammo_slot->SetAlignment(FVector2D(0.0, 0.5));
		ammo_slot->SetPosition(FVector2D(DIVIDER_GAP, 0.0));
		ammo_slot->SetAutoSize(true);

		slash_text_ = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EquipmentSlash"));
		slash_text_->SetText(FText::AsCultureInvariant(TEXT("/")));
		slash_text_->SetFont(GetGameFont(27));
		slash_text_->SetColorAndOpacity(FSlateColor(FLinearColor(0.34f, 0.36f, 0.37f)));
		UCanvasPanelSlot* slash_slot = panel->AddChildToCanvas(slash_text_);
		slash_slot->SetAnchors(FAnchors(0.72f, 0.52f));
		slash_slot->SetAlignment(FVector2D(0.0, 0.5));
		slash_slot->SetPosition(FVector2D::ZeroVector);
		slash_slot->SetAutoSize(true);

		total_text_ = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("EquipmentTotal"));
		total_text_->SetFont(GetGameFont(27));
		total_text_->SetColorAndOpacity(FSlateColor(FLinearColor(0.71f, 0.70f, 0.65f)));
		UCanvasPanelSlot* total_slot = panel->AddChildToCanvas(total_text_);
		total_slot->SetAnchors(FAnchors(0.80f, 0.52f));
		total_slot->SetAlignment(FVector2D(0.0, 0.5));
		total_slot->SetPosition(FVector2D::ZeroVector);
		total_slot->SetAutoSize(true);
	}
	TSharedRef<SWidget> widget = Super::RebuildWidget();
	SetEquipment(display_);
	return widget;
}

void UEquipmentWidget::SetEquipment(const FEquipmentDisplay& display)
{
	display_ = display;
	if (crosshair_)
	{
		crosshair_->SetVisibility(display.should_show_crosshair ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (!name_text_ || !ammo_text_ || !total_text_ || !slash_text_ || !divider_)
	{
		return;
	}
	name_text_->SetText(display.name);
	UCanvasPanelSlot* name_slot = CastChecked<UCanvasPanelSlot>(name_text_->Slot);
	// 탄을 쓰는 장비는 이름 길이와 관계없이 구분선까지 같은 간격을 두도록 이름 오른쪽 끝을 구분선 앞에 맞춘다.
	name_slot->SetAnchors(FAnchors(display.uses_ammo ? DIVIDER_ANCHOR : 0.5f, 0.5f));
	name_slot->SetAlignment(FVector2D(display.uses_ammo ? 1.0 : 0.5, 0.5));
	name_slot->SetPosition(FVector2D(display.uses_ammo ? -DIVIDER_GAP : 0.0, 0.0));
	const ESlateVisibility ammo_visibility = display.uses_ammo ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	// 오른쪽 값이 비어 있으면(다 찬 곡괭이) 사선과 함께 숨긴다.
	const ESlateVisibility total_visibility = display.uses_ammo && !display.total_ammo.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
	ammo_text_->SetVisibility(ammo_visibility);
	total_text_->SetVisibility(total_visibility);
	slash_text_->SetVisibility(total_visibility);
	divider_->SetVisibility(ammo_visibility);
	ammo_text_->SetText(FText::AsNumber(display.rounds));
	total_text_->SetText(display.total_ammo);
	ammo_text_->SetColorAndOpacity(FSlateColor(display.is_reloading ? FLinearColor(1.0f, 0.58f, 0.08f) : FLinearColor(1.0f, 0.94f, 0.79f)));
}
