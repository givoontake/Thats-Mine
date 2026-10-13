#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameTimeWidget.generated.h"

class UTextBlock;

// 게임 중 화면 오른쪽 위에 게임 제한시간까지 남은 시간(시:분:초)을 표시한다. 시간이 다 되었을 때의 처리는 이 위젯에서 하지 않는다.
UCLASS()
class THATSMINE_API UGameTimeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 지금부터 seconds초 동안 남은 시간을 줄여 가며 표시한다.
	void StartCountdown(float seconds);
	// 남은 시간을 더 줄이지 않고 지금 값으로 둔다.
	void StopCountdown();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& geometry, float delta_time) override;

private:
	static constexpr int32 FONT_SIZE = 24;
	// 화면 오른쪽 위 끝에서 띄우는 거리
	static constexpr float SCREEN_MARGIN = 28.0f;

	void UpdateTimeText();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> time_text_;

	double end_time_ = 0.0;
	int32 shown_seconds_ = INDEX_NONE;
	bool is_counting_ = false;
};
