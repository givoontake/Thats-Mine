#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Player/inventory_hint.h"
#include "HintPopupWidget.generated.h"

class UBorder;
class UHorizontalBox;
class UTextBlock;

// 힌트·아이템·탄약 획득 안내. 화면 오른쪽 아래 탄 표시 위에 "새 힌트 획득!" 같은 제목을 띄우고, 그 아래로 문장(아이템은 등급도)을 담은 패널이 오른쪽에서 밀려 들어온다.
// 받은 안내는 줄을 세워 하나씩 DISPLAY_SECONDS 동안 보이고, 끝날 때 패널은 다시 오른쪽으로 밀려 나가고 제목은 흐려진 뒤 다음 안내를 보인다.
UCLASS()
class THATSMINE_API UHintPopupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 등급이 없으면 문장만 보인다.
	void Enqueue(const FText& text, const FText& title, TOptional<EHintGrade> grade);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& geometry, float delta_time) override;

private:
	static constexpr float DISPLAY_SECONDS = 4.0f;
	static constexpr float TITLE_FADE_IN_SECONDS = 0.2f;
	static constexpr float SLIDE_SECONDS = 0.3f;
	static constexpr float FADE_OUT_SECONDS = 0.3f;
	// 패널이 들어오기 전과 나간 뒤 오른쪽으로 비켜 있는 거리. 화면 밖에서 들어오고 화면 밖으로 나가게 패널 폭보다 길다.
	static constexpr float SLIDE_DISTANCE = 560.0f;

	void ShowNext();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> title_;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> panel_;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> content_;

	struct FPopupEntry
	{
		FText text;
		FText title;
		TOptional<EHintGrade> grade;
	};
	TArray<FPopupEntry> queue_;
	// 지금 안내를 보인 시간. 보이는 안내가 없으면 음수다.
	float elapsed_ = -1.0f;
	// 새 내용을 넣은 뒤 배치가 끝날 때까지 기다릴 남은 프레임 수. 줄바꿈 글자는 배치에 몇 프레임이 걸려 그동안 빈 패널이 보이지 않게 한다.
	static constexpr int32 LAYOUT_WAIT_FRAMES = 3;
	int32 layout_wait_frames_ = 0;
};
