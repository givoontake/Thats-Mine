#include "InteractPromptWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Settings/KeyBindingSubsystem.h"
#include "Styling/CoreStyle.h"

TSharedRef<SWidget> UInteractPromptWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		UCanvasPanel* root = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = root;
		// 밝은 하늘이나 바닥 위에서도 읽히게 진한 외곽선을 두른다.
		FSlateFontInfo font = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), FONT_SIZE);
		font.OutlineSettings.OutlineSize = 2;
		font.OutlineSettings.OutlineColor = FLinearColor(0.03f, 0.02f, 0.01f, 0.9f);

		key_text_ = WidgetTree->ConstructWidget<UTextBlock>();
		key_text_->SetFont(font);
		key_text_->SetJustification(ETextJustify::Center);
		UBorder* key_box = WidgetTree->ConstructWidget<UBorder>();
		key_box->SetBrushColor(FLinearColor(0.03f, 0.02f, 0.01f, 0.75f));
		key_box->SetPadding(FMargin(12.0f, 2.0f));
		key_box->SetContent(key_text_);

		UTextBlock* label = WidgetTree->ConstructWidget<UTextBlock>();
		label->SetFont(font);
		label->SetText(NSLOCTEXT("Interaction", "Prompt", "상호작용"));

		UHorizontalBox* row = WidgetTree->ConstructWidget<UHorizontalBox>();
		row->AddChildToHorizontalBox(key_box)->SetVerticalAlignment(VAlign_Center);
		UHorizontalBoxSlot* label_slot = row->AddChildToHorizontalBox(label);
		label_slot->SetVerticalAlignment(VAlign_Center);
		label_slot->SetPadding(FMargin(10.0f, 0.0f, 0.0f, 0.0f));

		UCanvasPanelSlot* row_slot = root->AddChildToCanvas(row);
		row_slot->SetAnchors(FAnchors(0.5f, 0.5f));
		row_slot->SetAlignment(FVector2D(0.5, 0.0));
		row_slot->SetPosition(FVector2D(0.0, CENTER_OFFSET));
		row_slot->SetAutoSize(true);
	}
	return Super::RebuildWidget();
}

void UInteractPromptWidget::NativeConstruct()
{
	Super::NativeConstruct();
	key_text_->SetText(GetGameInstance()->GetSubsystem<UKeyBindingSubsystem>()->GetKey(EKeyAction::INTERACT).GetDisplayName());
}
