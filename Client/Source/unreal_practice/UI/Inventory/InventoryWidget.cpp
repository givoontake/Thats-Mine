#include "InventoryWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Player/PlayerCharacter.h"
#include "Styling/CoreStyle.h"

UInventoryWidget::UInventoryWidget(const FObjectInitializer& object_initializer)
	: Super(object_initializer)
{
	SetIsFocusable(true);
	frame_texture_ = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Thats_Mine/UI/Inventory/T_InventoryFrame.T_InventoryFrame")));
	equipment_icons_ = {
		TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Thats_Mine/UI/Inventory/T_SniperIcon.T_SniperIcon"))),
		TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Thats_Mine/UI/Inventory/T_PistolIcon.T_PistolIcon"))),
		TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Thats_Mine/UI/Inventory/T_PickaxeIcon.T_PickaxeIcon")))
	};
}

UTextBlock* UInventoryWidget::AddText(UCanvasPanel* canvas, const FText& text, int32 size, const FVector2D& position, const FVector2D& dimensions)
{
	UTextBlock* label = WidgetTree->ConstructWidget<UTextBlock>();
	label->SetText(text);
	label->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), size));
	label->SetColorAndOpacity(FLinearColor(0.96f, 0.90f, 0.78f));
	label->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanelSlot* slot = canvas->AddChildToCanvas(label);
	slot->SetPosition(position);
	slot->SetSize(dimensions);
	return label;
}

UImage* UInventoryWidget::AddPanel(UCanvasPanel* canvas, const FVector2D& position, const FVector2D& dimensions)
{
	UImage* panel = WidgetTree->ConstructWidget<UImage>();
	panel->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.022f, 0.016f, 0.010f, 0.72f), 4.0f, FLinearColor(0.18f, 0.12f, 0.075f), 1.5f));
	panel->SetVisibility(ESlateVisibility::HitTestInvisible);
	UCanvasPanelSlot* slot = canvas->AddChildToCanvas(panel);
	slot->SetPosition(position);
	slot->SetSize(dimensions);
	return panel;
}

TSharedRef<SWidget> UInventoryWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		UCanvasPanel* root = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = root;
		UImage* dim = WidgetTree->ConstructWidget<UImage>();
		dim->SetBrush(FSlateColorBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.28f)));
		UCanvasPanelSlot* dim_slot = root->AddChildToCanvas(dim);
		dim_slot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		dim_slot->SetOffsets(FMargin(0.0f));

		UScaleBox* scale = WidgetTree->ConstructWidget<UScaleBox>();
		scale->SetStretch(EStretch::ScaleToFit);
		scale->SetStretchDirection(EStretchDirection::DownOnly);
		UCanvasPanelSlot* scale_slot = root->AddChildToCanvas(scale);
		scale_slot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		scale_slot->SetOffsets(FMargin(24.0f));
		USizeBox* size = WidgetTree->ConstructWidget<USizeBox>();
		size->SetWidthOverride(1200.0f);
		size->SetHeightOverride(900.0f);
		UScaleBoxSlot* size_slot = Cast<UScaleBoxSlot>(scale->AddChild(size));
		size_slot->SetHorizontalAlignment(HAlign_Center);
		size_slot->SetVerticalAlignment(VAlign_Center);
		UCanvasPanel* panel = WidgetTree->ConstructWidget<UCanvasPanel>();
		size->AddChild(panel);
		UImage* frame = WidgetTree->ConstructWidget<UImage>();
		frame->SetBrushFromTexture(frame_texture_.LoadSynchronous());
		frame->SetVisibility(ESlateVisibility::HitTestInvisible);
		UCanvasPanelSlot* frame_slot = panel->AddChildToCanvas(frame);
		frame_slot->SetSize(FVector2D(1200.0, 900.0));
		AddText(panel, NSLOCTEXT("Inventory", "Title", "인벤토리"), 38, FVector2D(100.0, 43.0), FVector2D(700.0, 56.0));
		AddText(panel, NSLOCTEXT("Inventory", "Equipment", "장비"), 28, FVector2D(100.0, 120.0), FVector2D(300.0, 42.0));

		for (int32 index = 0; index < UE_ARRAY_COUNT(EQUIPMENT_ORDER); ++index)
		{
			const double x = 100.0 + index * 340.0;
			equipment_frames_.Add(AddPanel(panel, FVector2D(x, 170.0), FVector2D(320.0, 184.0)));
			AddPanel(panel, FVector2D(x + 12.0, 182.0), FVector2D(38.0, 38.0));
			UTextBlock* key = AddText(panel, FText::AsNumber(index + 1), 23, FVector2D(x + 12.0, 184.0), FVector2D(38.0, 34.0));
			key->SetJustification(ETextJustify::Center);
			equipment_names_.Add(AddText(panel, FText::GetEmpty(), 23, FVector2D(x + 66.0, 184.0), FVector2D(242.0, 38.0)));
			UImage* icon = WidgetTree->ConstructWidget<UImage>();
			icon->SetBrushFromTexture(equipment_icons_[index].LoadSynchronous());
			icon->SetVisibility(ESlateVisibility::HitTestInvisible);
			UCanvasPanelSlot* icon_slot = panel->AddChildToCanvas(icon);
			icon_slot->SetPosition(FVector2D(x + 48.0, 223.0));
			icon_slot->SetSize(FVector2D(224.0, 112.0));
			UTextBlock* ammo = AddText(panel, FText::GetEmpty(), 19, FVector2D(x + 10.0, 321.0), FVector2D(298.0, 30.0));
			ammo->SetJustification(ETextJustify::Right);
			equipment_ammo_.Add(ammo);
		}
		hint_count_ = AddText(panel, FText::GetEmpty(), 28, FVector2D(100.0, 389.0), FVector2D(980.0, 44.0));
		hint_list_ = WidgetTree->ConstructWidget<UScrollBox>();
		hint_list_->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
		hint_list_->SetScrollbarThickness(FVector2D(10.0, 10.0));
		UCanvasPanelSlot* list_slot = panel->AddChildToCanvas(hint_list_);
		list_slot->SetPosition(FVector2D(100.0, 447.0));
		list_slot->SetSize(FVector2D(1000.0, 367.0));
	}
	return Super::RebuildWidget();
}

void UInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	character_ = Cast<APlayerCharacter>(GetOwningPlayerPawn());
	if (APlayerCharacter* character = character_.Get())
	{
		equipment_changed_handle_ = character->on_equipment_changed_.AddUObject(this, &UInventoryWidget::RefreshEquipment);
		hints_changed_handle_ = character->on_hints_changed_.AddUObject(this, &UInventoryWidget::RefreshHints);
	}
	RefreshEquipment();
	RefreshHints();
}

void UInventoryWidget::NativeDestruct()
{
	if (APlayerCharacter* character = character_.Get())
	{
		character->on_equipment_changed_.Remove(equipment_changed_handle_);
		character->on_hints_changed_.Remove(hints_changed_handle_);
		character->MarkHintsRead();
	}
	character_.Reset();
	Super::NativeDestruct();
}

void UInventoryWidget::RefreshEquipment()
{
	const APlayerCharacter* character = character_.Get();
	if (!character) { return; }
	for (int32 index = 0; index < UE_ARRAY_COUNT(EQUIPMENT_ORDER); ++index)
	{
		const EWeaponType weapon = EQUIPMENT_ORDER[index];
		const FEquipmentDisplay display = character->GetWeaponDisplay(weapon);
		equipment_names_[index]->SetText(display.name);
		equipment_ammo_[index]->SetText(weapon == EWeaponType::PICKAXE
			? FText::Format(NSLOCTEXT("Inventory", "Charges", "탄약 {0}"), FText::AsNumber(character->GetPickaxeCharges()))
			: FText::Format(NSLOCTEXT("Inventory", "Ammo", "탄약 {0} / {1}"), FText::AsNumber(display.rounds), FText::AsNumber(display.capacity)));
		const bool is_selected = character->GetCurrentWeapon() == weapon;
		equipment_frames_[index]->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.022f, 0.016f, 0.010f, 0.72f), 4.0f,
			is_selected ? FLinearColor(1.0f, 0.61f, 0.12f) : FLinearColor(0.18f, 0.12f, 0.075f), is_selected ? 2.5f : 1.5f));
	}
}

void UInventoryWidget::RefreshHints()
{
	const APlayerCharacter* character = character_.Get();
	if (!character) { return; }
	TArray<FInventoryHint> hints = character->GetAcquiredHints();
	hints.StableSort([](const FInventoryHint& left, const FInventoryHint& right) { return left.grade < right.grade; });
	hint_count_->SetText(FText::Format(NSLOCTEXT("Inventory", "HintCount", "획득한 힌트 · {0}개"), FText::AsNumber(hints.Num())));
	const float offset = hint_list_->GetScrollOffset();
	hint_list_->ClearChildren();
	if (hints.IsEmpty())
	{
		UTextBlock* empty = WidgetTree->ConstructWidget<UTextBlock>();
		empty->SetText(NSLOCTEXT("Inventory", "Empty", "획득한 힌트가 없습니다."));
		empty->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 22));
		empty->SetColorAndOpacity(FLinearColor(0.65f, 0.60f, 0.51f));
		empty->SetJustification(ETextJustify::Center);
		Cast<UScrollBoxSlot>(hint_list_->AddChild(empty))->SetPadding(FMargin(20.0f, 95.0f));
	}
	for (const FInventoryHint& hint : hints)
	{
		UBorder* row = WidgetTree->ConstructWidget<UBorder>();
		row->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.04f, 0.03f, 0.02f, 0.85f), 4.0f, FLinearColor(0.18f, 0.12f, 0.075f), 1.0f));
		row->SetPadding(FMargin(14.0f, 8.0f));
		UScrollBoxSlot* row_slot = Cast<UScrollBoxSlot>(hint_list_->AddChild(row));
		row_slot->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 5.0f));
		UHorizontalBox* content = WidgetTree->ConstructWidget<UHorizontalBox>();
		row->AddChild(content);
		const FLinearColor grade_color = hint.grade == EHintGrade::A ? FLinearColor(1.0f, 0.64f, 0.17f)
			: hint.grade == EHintGrade::B ? FLinearColor(0.21f, 0.70f, 0.60f) : FLinearColor(0.65f, 0.65f, 0.65f);
		USizeBox* badge_size = WidgetTree->ConstructWidget<USizeBox>();
		badge_size->SetWidthOverride(42.0f);
		badge_size->SetHeightOverride(42.0f);
		content->AddChildToHorizontalBox(badge_size)->SetVerticalAlignment(VAlign_Center);
		UBorder* badge = WidgetTree->ConstructWidget<UBorder>();
		badge->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.035f, 0.027f, 0.020f), 3.0f, grade_color, 2.5f));
		badge->SetHorizontalAlignment(HAlign_Center);
		badge->SetVerticalAlignment(VAlign_Center);
		badge_size->AddChild(badge);
		UTextBlock* grade = WidgetTree->ConstructWidget<UTextBlock>();
		grade->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 26));
		grade->SetColorAndOpacity(grade_color);
		grade->SetText(FText::AsCultureInvariant(hint.grade == EHintGrade::A ? TEXT("A") : hint.grade == EHintGrade::B ? TEXT("B") : TEXT("C")));
		badge->AddChild(grade);
		UTextBlock* text = WidgetTree->ConstructWidget<UTextBlock>();
		text->SetText(hint.text);
		text->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 22));
		text->SetColorAndOpacity(FLinearColor(0.96f, 0.90f, 0.78f));
		text->SetAutoWrapText(true);
		UHorizontalBoxSlot* text_slot = content->AddChildToHorizontalBox(text);
		text_slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		text_slot->SetPadding(FMargin(22.0f, 5.0f, 16.0f, 5.0f));
		text_slot->SetVerticalAlignment(VAlign_Center);
		if (hint.is_new)
		{
			UTextBlock* fresh = WidgetTree->ConstructWidget<UTextBlock>();
			fresh->SetText(FText::AsCultureInvariant(TEXT("NEW")));
			fresh->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 17));
			fresh->SetColorAndOpacity(FLinearColor(1.0f, 0.64f, 0.17f));
			content->AddChildToHorizontalBox(fresh)->SetVerticalAlignment(VAlign_Center);
		}
	}
	hint_list_->SetScrollOffset(offset);
}

FReply UInventoryWidget::NativeOnPreviewKeyDown(const FGeometry& geometry, const FKeyEvent& event)
{
	if (event.GetKey() == EKeys::M)
	{
		if (!event.IsRepeat()) { on_map_requested_.ExecuteIfBound(); }
		return FReply::Handled();
	}
	if (event.GetKey() == EKeys::Tab || event.GetKey() == EKeys::Escape)
	{
		if (!event.IsRepeat()) { on_close_requested_.ExecuteIfBound(); }
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(geometry, event);
}

FReply UInventoryWidget::NativeOnKeyDown(const FGeometry& geometry, const FKeyEvent& event)
{
	const FKey key = event.GetKey();
	return key == EKeys::W || key == EKeys::A || key == EKeys::S || key == EKeys::D || key == EKeys::SpaceBar
		|| key == EKeys::One || key == EKeys::Two || key == EKeys::Three || key == EKeys::M ? FReply::Unhandled() : FReply::Handled();
}

FReply UInventoryWidget::NativeOnMouseButtonDown(const FGeometry& geometry, const FPointerEvent& event)
{
	return FReply::Handled().SetUserFocus(TakeWidget());
}

FReply UInventoryWidget::NativeOnMouseButtonUp(const FGeometry& geometry, const FPointerEvent& event)
{
	return FReply::Handled();
}
