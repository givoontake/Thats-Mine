#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateTypes.h"
#include "CycleOptionRow.generated.h"

class UButton;
class UTextBlock;

DECLARE_DELEGATE_OneParam(FOnCycleOptionStepped, int32);
DECLARE_DELEGATE(FOnCycleOptionHovered);

// 설정 창의 한 줄. 항목 이름, 이전·다음 버튼과 지금 값이 있다.
// 버튼을 누르면 이동 방향(-1, +1)만 알리고, 바꿀 값은 주인이 정해 SetOption이나 SetCustomText로 다시 넣는다.
// 마우스가 줄에 올라오면 알려 설명을 보이게 한다. 버튼 모양은 설정 창의 탭 버튼과 같다.
UCLASS()
class THATSMINE_API UCycleOptionRow : public UUserWidget
{
	GENERATED_BODY()

public:
	void Setup(const FText& label, const TArray<FText>& option_names, const FSlateFontInfo& label_font, const FSlateFontInfo& value_font, const FButtonStyle& button_style, const FSlateBrush& selected_brush);
	void SetOption(int32 option);
	// 선택지에 없는 값(예: 사용자 지정)을 보인다.
	void SetCustomText(const FText& text);
	int32 GetOption() const { return option_; }

	FOnCycleOptionStepped on_option_stepped_;
	FOnCycleOptionHovered on_hovered_;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeOnMouseEnter(const FGeometry& geometry, const FPointerEvent& event) override;

private:
	UFUNCTION()
	void SelectPrevious();
	UFUNCTION()
	void SelectNext();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> value_text_;

	FText label_;
	TArray<FText> option_names_;
	FText custom_text_;
	FSlateFontInfo label_font_;
	FSlateFontInfo value_font_;
	FButtonStyle button_style_;
	FSlateBrush selected_brush_;
	int32 option_ = 0;
};
