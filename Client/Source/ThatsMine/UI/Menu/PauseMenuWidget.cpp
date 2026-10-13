#include "PauseMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Materials/MaterialInterface.h"
#include "Styling/CoreStyle.h"

// 나무 버튼 재질 하나로 버튼 크기의 그림 붓을 만든다.
static FSlateBrush MakeButtonBrush(const TCHAR* material_path, const FVector2D& size)
{
	FSlateBrush brush;
	brush.SetResourceObject(LoadObject<UMaterialInterface>(nullptr, material_path));
	brush.ImageSize = size;
	return brush;
}

UPauseMenuWidget::UPauseMenuWidget(const FObjectInitializer& object_initializer)
	: Super(object_initializer)
{
	SetIsFocusable(true);
}

TSharedRef<SWidget> UPauseMenuWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		const FVector2D button_size(BUTTON_WIDTH, BUTTON_HEIGHT);
		button_style_.SetNormal(MakeButtonBrush(TEXT("/Game/Thats_Mine/UI/Shared/MI_ButtonWoodPlain.MI_ButtonWoodPlain"), button_size));
		button_style_.SetHovered(MakeButtonBrush(TEXT("/Game/Thats_Mine/UI/Shared/MI_ButtonWoodHoverOrange.MI_ButtonWoodHoverOrange"), button_size));
		button_style_.SetPressed(MakeButtonBrush(TEXT("/Game/Thats_Mine/UI/Shared/MI_ButtonWoodPressedOrange.MI_ButtonWoodPressedOrange"), button_size));
		button_style_.SetNormalForeground(FLinearColor(0.96f, 0.90f, 0.78f));
		button_style_.SetHoveredForeground(FLinearColor::White);
		button_style_.SetPressedForeground(FLinearColor(1.0f, 0.84f, 0.45f));

		UCanvasPanel* root = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = root;
		UImage* dim = WidgetTree->ConstructWidget<UImage>();
		dim->SetBrush(FSlateColorBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.35f)));
		UCanvasPanelSlot* dim_slot = root->AddChildToCanvas(dim);
		dim_slot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		dim_slot->SetOffsets(FMargin(0.0f));

		// 인벤토리 칸과 같은 어두운 판 위에 제목과 버튼을 세로로 놓는다.
		UBorder* panel = WidgetTree->ConstructWidget<UBorder>();
		panel->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.04f, 0.03f, 0.02f, 0.92f), 8.0f, FLinearColor(0.55f, 0.32f, 0.12f), 2.0f));
		panel->SetPadding(FMargin(40.0f, 28.0f, 40.0f, 36.0f));
		UCanvasPanelSlot* panel_slot = root->AddChildToCanvas(panel);
		panel_slot->SetAnchors(FAnchors(0.5f, 0.5f));
		panel_slot->SetAlignment(FVector2D(0.5, 0.5));
		panel_slot->SetAutoSize(true);
		UVerticalBox* list = WidgetTree->ConstructWidget<UVerticalBox>();
		panel->SetContent(list);
		UTextBlock* title = WidgetTree->ConstructWidget<UTextBlock>();
		title->SetText(NSLOCTEXT("PauseMenu", "Title", "메뉴"));
		title->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 32));
		title->SetColorAndOpacity(FLinearColor(1.0f, 0.85f, 0.35f));
		title->SetJustification(ETextJustify::Center);
		UVerticalBoxSlot* title_slot = list->AddChildToVerticalBox(title);
		title_slot->SetHorizontalAlignment(HAlign_Center);
		title_slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 18.0f));
		AddButton(list, NSLOCTEXT("PauseMenu", "Resume", "계속하기"))->OnClicked.AddDynamic(this, &UPauseMenuWidget::RequestResume);
		AddButton(list, NSLOCTEXT("PauseMenu", "Settings", "설정"))->OnClicked.AddDynamic(this, &UPauseMenuWidget::RequestSettings);
		leave_button_ = AddButton(list, NSLOCTEXT("PauseMenu", "Leave", "로비로 나가기"));
		leave_button_->OnClicked.AddDynamic(this, &UPauseMenuWidget::RequestLeave);
		AddButton(list, NSLOCTEXT("PauseMenu", "Quit", "게임 종료"))->OnClicked.AddDynamic(this, &UPauseMenuWidget::RequestQuit);
		SetCanLeaveToLobby(can_leave_to_lobby_);
	}
	return Super::RebuildWidget();
}

UButton* UPauseMenuWidget::AddButton(UVerticalBox* list, const FText& text)
{
	USizeBox* size = WidgetTree->ConstructWidget<USizeBox>();
	size->SetWidthOverride(BUTTON_WIDTH);
	size->SetHeightOverride(BUTTON_HEIGHT);
	UButton* button = WidgetTree->ConstructWidget<UButton>();
	button->SetStyle(button_style_);
	size->AddChild(button);
	UTextBlock* label = WidgetTree->ConstructWidget<UTextBlock>();
	label->SetText(text);
	label->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 24));
	label->SetColorAndOpacity(FSlateColor::UseForeground());
	Cast<UButtonSlot>(button->AddChild(label))->SetPadding(FMargin(4.0f, 0.5f, 4.0f, 3.5f));
	UVerticalBoxSlot* slot = list->AddChildToVerticalBox(size);
	slot->SetHorizontalAlignment(HAlign_Center);
	slot->SetPadding(FMargin(0.0f, 7.0f));
	return button;
}

void UPauseMenuWidget::SetCanLeaveToLobby(bool can_leave)
{
	can_leave_to_lobby_ = can_leave;
	if (leave_button_)
	{
		// 크기 상자째 숨겨 버튼 사이가 비지 않게 한다.
		leave_button_->GetParent()->SetVisibility(can_leave ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

FReply UPauseMenuWidget::NativeOnPreviewKeyDown(const FGeometry& geometry, const FKeyEvent& event)
{
	if (event.GetKey() == EKeys::Escape)
	{
		if (!event.IsRepeat()) { on_resume_requested_.ExecuteIfBound(); }
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(geometry, event);
}

FReply UPauseMenuWidget::NativeOnMouseButtonDown(const FGeometry& geometry, const FPointerEvent& event)
{
	return FReply::Handled().SetUserFocus(TakeWidget());
}

void UPauseMenuWidget::RequestResume()
{
	on_resume_requested_.ExecuteIfBound();
}

void UPauseMenuWidget::RequestSettings()
{
	on_settings_requested_.ExecuteIfBound();
}

void UPauseMenuWidget::RequestLeave()
{
	on_leave_requested_.ExecuteIfBound();
}

void UPauseMenuWidget::RequestQuit()
{
	on_quit_requested_.ExecuteIfBound();
}
