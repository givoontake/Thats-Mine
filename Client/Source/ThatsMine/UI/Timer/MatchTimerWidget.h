#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MatchTimerWidget.generated.h"

class UTextBlock;
class UVerticalBox;

// 화면 중앙 상단에 남은 시간과 안내 글씨를 표시한다. 시간이 다 되었을 때의 처리는 이 위젯에서 하지 않는다.
UCLASS()
class THATSMINE_API UMatchTimerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 지금부터 seconds초 동안 남은 시간을 줄여 가며 표시한다.
	UFUNCTION(BlueprintCallable, Category = "Match Timer")
	void StartCountdown(float seconds);

	UFUNCTION(BlueprintCallable, Category = "Match Timer")
	void HideTimer();

	static constexpr int32 MESSAGE_FONT_SIZE = 20;

	// 빈 글씨를 넣으면 안내 글씨를 숨긴다. 글씨 크기 기본값은 MESSAGE_FONT_SIZE다(블루프린트 함수 기본값은 상수 이름을 쓸 수 없다).
	UFUNCTION(BlueprintCallable, Category = "Match Timer")
	void SetMessage(const FText& message, int32 font_size = 20);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& geometry, float delta_time) override;

	// 이 시간 이하로 남으면 강조 색으로 바꾸고 1초마다 살짝 커졌다 돌아온다.
	UPROPERTY(EditDefaultsOnly, Category = "Match Timer", meta = (ClampMin = "0"))
	int32 warning_seconds_ = 5;

	UPROPERTY(EditDefaultsOnly, Category = "Match Timer")
	FLinearColor text_color_ = FLinearColor(1.0f, 0.94f, 0.79f);

	UPROPERTY(EditDefaultsOnly, Category = "Match Timer")
	FLinearColor warning_color_ = FLinearColor::FromSRGBColor(FColor(255, 138, 36));

private:
	void UpdateTimerText();

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> content_;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> timer_text_;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> message_text_;

	double end_time_ = 0.0;
	int32 shown_seconds_ = INDEX_NONE;
	bool is_counting_ = false;
};
