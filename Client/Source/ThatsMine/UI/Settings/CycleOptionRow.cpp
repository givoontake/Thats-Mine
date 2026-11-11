#include "CycleOptionRow.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "SettingsWidget.h"

// 이름 칸 너비와 버튼 높이는 설정 창 모든 탭의 줄과 같다. 값 칸은 두 화살표 버튼 사이 남은 너비를 채운다.
static constexpr float ARROW_BUTTON_WIDTH = 70.0f;

void UCycleOptionRow::Setup(const FText& label, const TArray<FText>& option_names, const FSlateFontInfo& label_font, const FSlateFontInfo& value_font, const FButtonStyle& button_style, const FSlateBrush& selected_brush)
{
	label_ = label;
	option_names_ = option_names;
	label_font_ = label_font;
	value_font_ = value_font;
	button_style_ = button_style;
	selected_brush_ = selected_brush;
	// 이름 칸 위에서도 마우스가 올라온 것을 알 수 있게 줄 전체가 마우스를 받는다.
	SetVisibility(ESlateVisibility::Visible);
}

void UCycleOptionRow::SetOption(int32 option)
{
	option_ = FMath::Clamp(option, 0, option_names_.Num() - 1);
	if (value_text_)
	{
		value_text_->SetText(option_names_[option_]);
	}
}

void UCycleOptionRow::SetCustomText(const FText& text)
{
	option_ = INDEX_NONE;
	custom_text_ = text;
	if (value_text_)
	{
		value_text_->SetText(custom_text_);
	}
}

TSharedRef<SWidget> UCycleOptionRow::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		UHorizontalBox* root = WidgetTree->ConstructWidget<UHorizontalBox>();
		WidgetTree->RootWidget = root;
		USizeBox* label_box = WidgetTree->ConstructWidget<USizeBox>();
		label_box->SetWidthOverride(USettingsWidget::OPTION_LABEL_WIDTH);
		UTextBlock* label = WidgetTree->ConstructWidget<UTextBlock>();
		label->SetFont(label_font_);
		label->SetText(label_);
		label_box->AddChild(label);
		UHorizontalBoxSlot* label_slot = root->AddChildToHorizontalBox(label_box);
		label_slot->SetVerticalAlignment(VAlign_Center);
		// 화살표 버튼은 탭 버튼보다 낮아 탭과 같은 위아래 여백이면 글자 칸이 글자 한 줄보다 작아져 글자가 아래로 처지므로, 위아래 여백을 두지 않는다.
		FButtonStyle arrow_style = button_style_;
		arrow_style.NormalPadding.Top = arrow_style.NormalPadding.Bottom = 0.0f;
		arrow_style.PressedPadding.Top = arrow_style.PressedPadding.Bottom = 0.0f;
		UButton* buttons[2] = {};
		for (int32 index = 0; index < 2; ++index)
		{
			USizeBox* button_box = WidgetTree->ConstructWidget<USizeBox>();
			button_box->SetWidthOverride(ARROW_BUTTON_WIDTH);
			button_box->SetHeightOverride(USettingsWidget::OPTION_BUTTON_HEIGHT);
			UButton* button = WidgetTree->ConstructWidget<UButton>();
			button->SetStyle(arrow_style);
			USettingsWidget::ApplyTabStyle(button, button_style_, selected_brush_, false);
			UTextBlock* text = WidgetTree->ConstructWidget<UTextBlock>();
			text->SetFont(value_font_);
			text->SetText(FText::FromString(index == 0 ? TEXT("<") : TEXT(">")));
			text->SetColorAndOpacity(FSlateColor::UseForeground());
			CastChecked<UButtonSlot>(button->AddChild(text))->SetPadding(FMargin(4.0f, 2.0f, 4.0f, 2.0f));
			button_box->AddChild(button);
			buttons[index] = button;
			if (index == 1)
			{
				value_text_ = WidgetTree->ConstructWidget<UTextBlock>();
				value_text_->SetFont(value_font_);
				value_text_->SetJustification(ETextJustify::Center);
				UHorizontalBoxSlot* value_slot = root->AddChildToHorizontalBox(value_text_);
				value_slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				value_slot->SetVerticalAlignment(VAlign_Center);
			}
			UHorizontalBoxSlot* button_slot = root->AddChildToHorizontalBox(button_box);
			button_slot->SetVerticalAlignment(VAlign_Center);
		}
		buttons[0]->OnClicked.AddDynamic(this, &UCycleOptionRow::SelectPrevious);
		buttons[1]->OnClicked.AddDynamic(this, &UCycleOptionRow::SelectNext);
		if (option_ == INDEX_NONE)
		{
			SetCustomText(custom_text_);
		}
		else
		{
			SetOption(option_);
		}
	}
	return Super::RebuildWidget();
}

void UCycleOptionRow::NativeOnMouseEnter(const FGeometry& geometry, const FPointerEvent& event)
{
	Super::NativeOnMouseEnter(geometry, event);
	on_hovered_.ExecuteIfBound();
}

void UCycleOptionRow::SelectPrevious()
{
	on_option_stepped_.ExecuteIfBound(-1);
}

void UCycleOptionRow::SelectNext()
{
	on_option_stepped_.ExecuteIfBound(1);
}
