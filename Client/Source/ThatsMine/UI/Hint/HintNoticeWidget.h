#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HintNoticeWidget.generated.h"

class UTextBlock;

// 화면 위쪽 가운데에 힌트 제공 안내 문구를 잠시 띄운다. 새 문구가 오면 바로 바꿔 처음부터 다시 보인다.
UCLASS()
class THATSMINE_API UHintNoticeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void ShowNotice(const FText& text);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& geometry, float delta_time) override;

private:
	static constexpr float DISPLAY_SECONDS = 3.0f;
	static constexpr float FADE_SECONDS = 0.3f;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> text_;

	// 지금 문구를 보인 시간. 보이는 문구가 없으면 음수다.
	float elapsed_ = -1.0f;
};
