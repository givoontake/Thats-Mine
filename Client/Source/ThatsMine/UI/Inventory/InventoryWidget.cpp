#include "InventoryWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
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
#include "Components/WrapBox.h"
#include "Components/WidgetSwitcher.h"
#include "Components/WrapBoxSlot.h"
#include "Materials/MaterialInterface.h"
#include "Player/PlayerCharacter.h"
#include "Settings/KeyBindingSubsystem.h"
#include "UI/Hint/HintGradeBadge.h"
#include "UI/Inventory/BlockBoardWidget.h"
#include "Styling/CoreStyle.h"

// 나무 버튼 재질 하나로 탭 버튼 크기의 그림 붓을 만든다.
static FSlateBrush MakeTabBrush(const TCHAR* material_path)
{
	FSlateBrush brush;
	brush.SetResourceObject(LoadObject<UMaterialInterface>(nullptr, material_path));
	brush.ImageSize = FVector2D(200.0, 52.0);
	return brush;
}

UInventoryWidget::UInventoryWidget(const FObjectInitializer& object_initializer)
	: Super(object_initializer)
{
	SetIsFocusable(true);
	frame_texture_ = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Thats_Mine/UI/Inventory/T_InventoryFrame.T_InventoryFrame")));
	equipment_icons_ = {
		TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Thats_Mine/UI/Inventory/T_SniperIcon.T_SniperIcon"))),
		TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Thats_Mine/UI/Inventory/T_PistolIcon.T_PistolIcon"))),
		TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Thats_Mine/UI/Inventory/T_PickaxeIcon.T_PickaxeIcon"))),
		TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Thats_Mine/UI/Inventory/T_MagnifierIcon.T_MagnifierIcon")))
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

		// 제목 아래에 탭 버튼을 두고, 그 아래를 탭 화면이 채운다. 탭 화면은 프레임 전체 크기의 캔버스라 좌표는 프레임 기준이다.
		tab_style_.SetNormal(MakeTabBrush(TEXT("/Game/Thats_Mine/UI/Shared/MI_ButtonWoodPlain.MI_ButtonWoodPlain")));
		tab_style_.SetHovered(MakeTabBrush(TEXT("/Game/Thats_Mine/UI/Shared/MI_ButtonWoodHoverOrange.MI_ButtonWoodHoverOrange")));
		tab_style_.SetPressed(MakeTabBrush(TEXT("/Game/Thats_Mine/UI/Shared/MI_ButtonWoodPressedOrange.MI_ButtonWoodPressedOrange")));
		tab_style_.SetNormalForeground(FLinearColor(0.96f, 0.90f, 0.78f));
		tab_style_.SetHoveredForeground(FLinearColor::White);
		tab_style_.SetPressedForeground(FLinearColor(1.0f, 0.84f, 0.45f));
		selected_tab_brush_ = MakeTabBrush(TEXT("/Game/Thats_Mine/UI/Shared/MI_ButtonWoodSelected.MI_ButtonWoodSelected"));
		const FText tab_names[] = { NSLOCTEXT("Inventory", "Equipment", "장비"), NSLOCTEXT("Inventory", "Items", "아이템"), NSLOCTEXT("Inventory", "Hints", "힌트") };
		for (int32 tab = 0; tab < UE_ARRAY_COUNT(tab_names); ++tab)
		{
			UButton* button = WidgetTree->ConstructWidget<UButton>();
			UTextBlock* label = WidgetTree->ConstructWidget<UTextBlock>();
			label->SetText(tab_names[tab]);
			label->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 24));
			label->SetColorAndOpacity(FSlateColor::UseForeground());
			Cast<UButtonSlot>(button->AddChild(label))->SetPadding(FMargin(4.0f, 0.5f, 4.0f, 3.5f));
			UCanvasPanelSlot* button_slot = panel->AddChildToCanvas(button);
			button_slot->SetPosition(FVector2D(100.0 + tab * 220.0, 118.0));
			button_slot->SetSize(FVector2D(200.0, 52.0));
			tab_buttons_.Add(button);
		}
		tab_buttons_[EQUIPMENT_TAB]->OnClicked.AddDynamic(this, &UInventoryWidget::SelectEquipmentTab);
		tab_buttons_[ITEM_TAB]->OnClicked.AddDynamic(this, &UInventoryWidget::SelectItemTab);
		tab_buttons_[HINT_TAB]->OnClicked.AddDynamic(this, &UInventoryWidget::SelectHintTab);
		pages_ = WidgetTree->ConstructWidget<UWidgetSwitcher>();
		UCanvasPanelSlot* pages_slot = panel->AddChildToCanvas(pages_);
		pages_slot->SetSize(FVector2D(1200.0, 900.0));
		UCanvasPanel* equipment_page = WidgetTree->ConstructWidget<UCanvasPanel>();
		UCanvasPanel* item_page = WidgetTree->ConstructWidget<UCanvasPanel>();
		UCanvasPanel* hint_page = WidgetTree->ConstructWidget<UCanvasPanel>();
		pages_->AddChild(equipment_page);
		pages_->AddChild(item_page);
		pages_->AddChild(hint_page);

		for (int32 index = 0; index < UE_ARRAY_COUNT(EQUIPMENT_ORDER); ++index)
		{
			const double x = 100.0 + index * 252.0;
			equipment_frames_.Add(AddPanel(equipment_page, FVector2D(x, 200.0), FVector2D(240.0, 184.0)));
			AddPanel(equipment_page, FVector2D(x + 12.0, 212.0), FVector2D(38.0, 38.0));
			UTextBlock* key = AddText(equipment_page, FText::AsNumber(index + 1), 23, FVector2D(x + 12.0, 214.0), FVector2D(38.0, 34.0));
			key->SetJustification(ETextJustify::Center);
			equipment_names_.Add(AddText(equipment_page, FText::GetEmpty(), 21, FVector2D(x + 62.0, 216.0), FVector2D(170.0, 36.0)));
			UImage* icon = WidgetTree->ConstructWidget<UImage>();
			icon->SetBrushFromTexture(equipment_icons_[index].LoadSynchronous());
			icon->SetVisibility(ESlateVisibility::HitTestInvisible);
			UCanvasPanelSlot* icon_slot = equipment_page->AddChildToCanvas(icon);
			icon_slot->SetPosition(FVector2D(x + 20.0, 255.0));
			icon_slot->SetSize(FVector2D(200.0, 100.0));
			UTextBlock* ammo = AddText(equipment_page, FText::GetEmpty(), 19, FVector2D(x + 10.0, 351.0), FVector2D(218.0, 30.0));
			ammo->SetJustification(ETextJustify::Right);
			equipment_ammo_.Add(ammo);
		}
		// 장비 아래에 블록 칸을 둔다.
		UBlockBoardWidget* block_board = WidgetTree->ConstructWidget<UBlockBoardWidget>();
		UCanvasPanelSlot* block_slot = equipment_page->AddChildToCanvas(block_board);
		block_slot->SetPosition(FVector2D(100.0, 404.0));
		block_slot->SetSize(FVector2D(UBlockBoardWidget::WIDTH, UBlockBoardWidget::HEIGHT));
		// 아이템은 두 줄로 나란히 놓는다.
		item_count_ = AddText(item_page, FText::GetEmpty(), 28, FVector2D(100.0, 196.0), FVector2D(980.0, 44.0));
		item_list_ = WidgetTree->ConstructWidget<UWrapBox>();
		item_list_->SetInnerSlotPadding(FVector2D(10.0, 5.0));
		UCanvasPanelSlot* item_slot = item_page->AddChildToCanvas(item_list_);
		item_slot->SetPosition(FVector2D(100.0, 248.0));
		item_slot->SetSize(FVector2D(1000.0, 560.0));
		hint_count_ = AddText(hint_page, FText::GetEmpty(), 28, FVector2D(100.0, 196.0), FVector2D(980.0, 44.0));
		hint_list_ = WidgetTree->ConstructWidget<UScrollBox>();
		hint_list_->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
		hint_list_->SetScrollbarThickness(FVector2D(10.0, 10.0));
		UCanvasPanelSlot* list_slot = hint_page->AddChildToCanvas(hint_list_);
		list_slot->SetPosition(FVector2D(100.0, 248.0));
		list_slot->SetSize(FVector2D(1000.0, 570.0));
	}
	return Super::RebuildWidget();
}

void UInventoryWidget::SelectEquipmentTab()
{
	SelectTab(EQUIPMENT_TAB);
}

void UInventoryWidget::SelectItemTab()
{
	SelectTab(ITEM_TAB);
}

void UInventoryWidget::SelectHintTab()
{
	SelectTab(HINT_TAB);
}

// 고른 탭 버튼은 선택 배경과 눌림 글자색으로 칠하고, 그 탭 화면을 보인다.
void UInventoryWidget::SelectTab(int32 tab)
{
	for (int32 index = 0; index < tab_buttons_.Num(); ++index)
	{
		FButtonStyle style = tab_style_;
		if (index == tab)
		{
			style.SetNormal(selected_tab_brush_);
			style.SetHovered(selected_tab_brush_);
			style.SetNormalForeground(tab_style_.PressedForeground);
			style.SetHoveredForeground(tab_style_.PressedForeground);
		}
		tab_buttons_[index]->SetStyle(style);
	}
	pages_->SetActiveWidgetIndex(tab);
}

void UInventoryWidget::NativeConstruct()
{
	Super::NativeConstruct();
	character_ = Cast<APlayerCharacter>(GetOwningPlayerPawn());
	if (APlayerCharacter* character = character_.Get())
	{
		equipment_changed_handle_ = character->on_equipment_changed_.AddUObject(this, &UInventoryWidget::RefreshEquipment);
		hints_changed_handle_ = character->on_hints_changed_.AddUObject(this, &UInventoryWidget::RefreshHints);
		items_changed_handle_ = character->on_items_changed_.AddUObject(this, &UInventoryWidget::RefreshItems);
	}
	RefreshEquipment();
	RefreshItems();
	RefreshHints();
	SelectTab(HINT_TAB);
}

void UInventoryWidget::NativeDestruct()
{
	if (APlayerCharacter* character = character_.Get())
	{
		character->on_equipment_changed_.Remove(equipment_changed_handle_);
		character->on_hints_changed_.Remove(hints_changed_handle_);
		character->on_items_changed_.Remove(items_changed_handle_);
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
		// 화면 아래 장비 표시와 같은 값이다. 총은 현재 탄 / 소지 탄, 곡괭이는 사용 가능 수와 충전 중이면 남은 시간이다. 돋보기는 넓은 탐지 돋보기 등급마다 가진 개수다.
		equipment_ammo_[index]->SetText(weapon == EWeaponType::MAGNIFIER
			? FText::Format(NSLOCTEXT("Inventory", "WideMagnifiers", "넓은 S {0} · A {1}"), FText::AsNumber(character->GetWideMagnifierCount(EHintGrade::S)), FText::AsNumber(character->GetWideMagnifierCount(EHintGrade::A)))
			: display.total_ammo.IsEmpty()
			? FText::Format(NSLOCTEXT("Inventory", "Charges", "탄약 {0}"), FText::AsNumber(display.rounds))
			: FText::Format(NSLOCTEXT("Inventory", "Ammo", "탄약 {0} / {1}"), FText::AsNumber(display.rounds), display.total_ammo));
		const bool is_selected = character->GetCurrentWeapon() == weapon;
		equipment_frames_[index]->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.022f, 0.016f, 0.010f, 0.72f), 4.0f,
			is_selected ? FLinearColor(1.0f, 0.61f, 0.12f) : FLinearColor(0.18f, 0.12f, 0.075f), is_selected ? 2.5f : 1.5f));
	}
}

void UInventoryWidget::RefreshItems()
{
	const APlayerCharacter* character = character_.Get();
	if (!character) { return; }
	const TArray<FInventoryItem> items = character->GetInventoryItems();
	item_count_->SetText(FText::Format(NSLOCTEXT("Inventory", "ItemCount", "획득한 아이템 · {0}개"), FText::AsNumber(items.Num())));
	item_list_->ClearChildren();
	if (items.IsEmpty())
	{
		UTextBlock* empty = WidgetTree->ConstructWidget<UTextBlock>();
		empty->SetText(NSLOCTEXT("Inventory", "EmptyItems", "획득한 아이템이 없습니다."));
		empty->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 22));
		empty->SetColorAndOpacity(FLinearColor(0.65f, 0.60f, 0.51f));
		item_list_->AddChildToWrapBox(empty)->SetPadding(FMargin(20.0f, 8.0f));
	}
	for (const FInventoryItem& item : items)
	{
		USizeBox* size = WidgetTree->ConstructWidget<USizeBox>();
		size->SetWidthOverride(490.0f);
		size->AddChild(CreateGradeRow(item.grade, item.text, false, 20));
		item_list_->AddChildToWrapBox(size);
	}
}

UBorder* UInventoryWidget::CreateGradeRow(EHintGrade grade, const FText& text, bool is_new, int32 font_size)
{
	UBorder* row = WidgetTree->ConstructWidget<UBorder>();
	row->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.04f, 0.03f, 0.02f, 0.85f), 4.0f, FLinearColor(0.18f, 0.12f, 0.075f), 1.0f));
	row->SetPadding(FMargin(14.0f, 8.0f));
	UHorizontalBox* content = WidgetTree->ConstructWidget<UHorizontalBox>();
	row->AddChild(content);
	UHintGradeBadge* badge = WidgetTree->ConstructWidget<UHintGradeBadge>();
	badge->SetGrade(grade);
	content->AddChildToHorizontalBox(badge)->SetVerticalAlignment(VAlign_Center);
	UTextBlock* label = WidgetTree->ConstructWidget<UTextBlock>();
	label->SetText(text);
	label->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), font_size));
	label->SetColorAndOpacity(FLinearColor(0.96f, 0.90f, 0.78f));
	label->SetAutoWrapText(true);
	UHorizontalBoxSlot* label_slot = content->AddChildToHorizontalBox(label);
	label_slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	label_slot->SetPadding(FMargin(22.0f, 5.0f, 16.0f, 5.0f));
	label_slot->SetVerticalAlignment(VAlign_Center);
	if (is_new)
	{
		UTextBlock* fresh = WidgetTree->ConstructWidget<UTextBlock>();
		fresh->SetText(FText::AsCultureInvariant(TEXT("NEW")));
		fresh->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 17));
		fresh->SetColorAndOpacity(FLinearColor(1.0f, 0.64f, 0.17f));
		content->AddChildToHorizontalBox(fresh)->SetVerticalAlignment(VAlign_Center);
	}
	return row;
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
		Cast<UScrollBoxSlot>(hint_list_->AddChild(empty))->SetPadding(FMargin(20.0f, 240.0f));
	}
	for (const FInventoryHint& hint : hints)
	{
		UScrollBoxSlot* row_slot = Cast<UScrollBoxSlot>(hint_list_->AddChild(CreateGradeRow(hint.grade, hint.text, hint.is_new, 22)));
		row_slot->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 5.0f));
	}
	hint_list_->SetScrollOffset(offset);
}

FReply UInventoryWidget::NativeOnPreviewKeyDown(const FGeometry& geometry, const FKeyEvent& event)
{
	const UKeyBindingSubsystem* bindings = GetGameInstance()->GetSubsystem<UKeyBindingSubsystem>();
	if (event.GetKey() == bindings->GetKey(EKeyAction::MAP) || event.GetKey() == bindings->GetKey(EKeyAction::HELP))
	{
		return FReply::Handled();
	}
	if (event.GetKey() == bindings->GetKey(EKeyAction::INVENTORY) || event.GetKey() == EKeys::Escape)
	{
		if (!event.IsRepeat()) { on_close_requested_.ExecuteIfBound(); }
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(geometry, event);
}

FReply UInventoryWidget::NativeOnKeyDown(const FGeometry& geometry, const FKeyEvent& event)
{
	// 이동·점프·무기 선택은 게임 입력으로 넘기고, 나머지 동작은 인벤토리에서 소비한다.
	const UKeyBindingSubsystem* bindings = GetGameInstance()->GetSubsystem<UKeyBindingSubsystem>();
	for (const EKeyAction action : { EKeyAction::MOVE_FORWARD, EKeyAction::MOVE_BACKWARD, EKeyAction::MOVE_LEFT, EKeyAction::MOVE_RIGHT, EKeyAction::JUMP,
		EKeyAction::WEAPON_SNIPER, EKeyAction::WEAPON_PISTOL, EKeyAction::WEAPON_PICKAXE, EKeyAction::WEAPON_MAGNIFIER })
	{
		if (event.GetKey() == bindings->GetKey(action))
		{
			return FReply::Unhandled();
		}
	}
	return FReply::Handled();
}

FReply UInventoryWidget::NativeOnMouseButtonDown(const FGeometry& geometry, const FPointerEvent& event)
{
	return FReply::Handled().SetUserFocus(TakeWidget());
}

FReply UInventoryWidget::NativeOnMouseButtonUp(const FGeometry& geometry, const FPointerEvent& event)
{
	return FReply::Handled();
}
