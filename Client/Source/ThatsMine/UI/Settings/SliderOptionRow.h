#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SliderOptionRow.generated.h"

class UEditableText;
class USizeBox;
class USlider;
class UTextBlock;

// 설정 창의 슬라이더 한 줄. 항목 이름, 직접 입력할 수 있는 숫자 칸, 슬라이더를 가로 상자 한 줄에 두고 세 칸 모두 줄의 세로 가운데에 맞춘다.
// 모양은 이 클래스를 부모로 한 위젯 블루프린트(WBP_SliderOptionRow)가 정하고, 항목 이름은 줄을 놓은 위젯에서 정한다.
// 슬라이더를 움직이면 숫자 칸을 고치고, 숫자 칸에 입력을 마치면 간격에 맞춰 반올림하고 범위 안으로 맞춰 슬라이더에 넣는다.
UCLASS(Abstract)
class THATSMINE_API USliderOptionRow : public UUserWidget
{
	GENERATED_BODY()

public:
	// 슬라이더 간격과, 숫자 칸에 보일 때 값에 곱할 배율과 소수 자리 수를 정한다.
	void Setup(float step, float display_scale, int32 fractional_digits);
	// 슬라이더 범위를 정한다. 정하지 않으면 모양에서 정한 범위를 쓴다.
	void SetRange(float min_value, float max_value);
	float GetValue() const;
	void SetValue(float value);

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeOnInitialized() override;

	// 항목 이름. 줄을 놓은 위젯에서 정한다.
	UPROPERTY(EditAnywhere, Category = "Option")
	FText label_;

	// 이름 칸. 너비는 설정 창 모든 탭의 줄과 같은 이름 칸 너비로 맞춘다.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USizeBox> label_box_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> label_text_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableText> value_text_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USlider> slider_;

private:
	UFUNCTION()
	void UpdateValueText(float value);
	UFUNCTION()
	void CommitValueText(const FText& text, ETextCommit::Type commit_type);

	float display_scale_ = 1.0f;
	int32 fractional_digits_ = 0;
};
