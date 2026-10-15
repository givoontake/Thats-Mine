#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AwardWidget.generated.h"

class AAwardStage;
class UButton;
class UCanvasPanelSlot;
class UTextBlock;
class UVerticalBox;

// 시상식 화면. 위쪽 가운데에 부문 제목이 위에서 내려오고, 시상대 캐릭터마다 머리 위에 닉네임과 그 아래 횟수를 띄우며, 오른쪽 아래에 건너뛰기 버튼을 둔다.
UCLASS()
class THATSMINE_API UAwardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 새 부문을 보인다. nicknames와 records(횟수 문장)는 1위부터이고 그 수만큼 이름표를 띄운다.
	void ShowScene(AAwardStage* stage, const FText& title, const TArray<FString>& nicknames, const TArray<FText>& records);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& geometry, float delta_time) override;

private:
	static constexpr int32 TITLE_FONT_SIZE = 48;
	static constexpr int32 NAME_FONT_SIZE = 22;
	static constexpr int32 RECORD_FONT_SIZE = 16;
	static constexpr int32 RANK_FONT_SIZE = 44;
	// 제목이 내려오는 시간(초)과 거리, 멈추는 위치(화면 위에서)
	static constexpr float TITLE_DROP_SECONDS = 0.4f;
	static constexpr float TITLE_DROP_DISTANCE = 160.0f;
	static constexpr float TITLE_TOP = 60.0f;
	static constexpr float SCREEN_MARGIN = 40.0f;

	UFUNCTION()
	void HandleSkipClicked();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> title_text_;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanelSlot> title_slot_;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> name_texts_;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> record_texts_;

	// 순위마다 닉네임과 횟수를 위아래로 묶은 이름표
	UPROPERTY(Transient)
	TArray<TObjectPtr<UVerticalBox>> name_tags_;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UCanvasPanelSlot>> name_slots_;

	// 시상대 앞면 순위 숫자
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> rank_texts_;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UCanvasPanelSlot>> rank_slots_;

	UPROPERTY(Transient)
	TObjectPtr<UButton> skip_button_;

	UPROPERTY(Transient)
	TObjectPtr<AAwardStage> stage_;

	// 제목이 내려오기 시작한 월드 시각(초)
	double title_start_time_ = 0.0;
	int32 name_count_ = 0;
};
