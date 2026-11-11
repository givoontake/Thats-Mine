#include "SliderOptionRow.h"

#include "Components/EditableText.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "SettingsWidget.h"

void USliderOptionRow::Setup(float step, float display_scale, int32 fractional_digits)
{
	slider_->SetStepSize(step);
	display_scale_ = display_scale;
	fractional_digits_ = fractional_digits;
	UpdateValueText(0.0f);
}

void USliderOptionRow::SetRange(float min_value, float max_value)
{
	slider_->SetMinValue(min_value);
	slider_->SetMaxValue(max_value);
}

float USliderOptionRow::GetValue() const
{
	return slider_->GetValue();
}

void USliderOptionRow::SetValue(float value)
{
	slider_->SetValue(value);
	UpdateValueText(0.0f);
}

void USliderOptionRow::NativePreConstruct()
{
	Super::NativePreConstruct();
	label_box_->SetWidthOverride(USettingsWidget::OPTION_LABEL_WIDTH);
	label_text_->SetText(label_);
}

void USliderOptionRow::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	slider_->OnValueChanged.AddDynamic(this, &USliderOptionRow::UpdateValueText);
	value_text_->SetSelectAllTextWhenFocused(true);
	value_text_->SetClearKeyboardFocusOnCommit(true);
	value_text_->OnTextCommitted.AddDynamic(this, &USliderOptionRow::CommitValueText);
}

void USliderOptionRow::UpdateValueText(float value)
{
	FNumberFormattingOptions format;
	format.SetMinimumFractionalDigits(fractional_digits_);
	format.SetMaximumFractionalDigits(fractional_digits_);
	value_text_->SetText(FText::AsNumber(slider_->GetValue() * display_scale_, &format));
}

// 입력한 숫자를 간격에 맞춰 반올림하고 범위 안으로 맞춰 슬라이더에 넣는다. 숫자가 아니거나 입력을 취소하면 표시만 지금 값으로 되돌린다.
void USliderOptionRow::CommitValueText(const FText& text, ETextCommit::Type commit_type)
{
	const FString input = text.ToString().TrimStartAndEnd();
	if (commit_type != ETextCommit::OnCleared && input.IsNumeric())
	{
		const float step = slider_->GetStepSize();
		slider_->SetValue(FMath::Clamp(FMath::RoundToFloat(FCString::Atof(*input) / display_scale_ / step) * step, slider_->GetMinValue(), slider_->GetMaxValue()));
	}
	UpdateValueText(0.0f);
}
