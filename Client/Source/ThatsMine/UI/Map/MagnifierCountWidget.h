#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MagnifierCountWidget.generated.h"

class UTextBlock;
class UVerticalBox;

// 내 캐릭터가 돋보기를 들고 있는 동안 화면 오른쪽 가운데에 발밑에서 COUNT_RADIUS 안에 있는 힌트 물체의 종류와 수를 보인다.
// 한 개 이상인 종류만 종류 순서로 쓰고, UPDATE_INTERVAL마다 다시 센다.
// 배제 판에 보물이 있을 칸이 하나만 남았으면, 가진 단서 힌트 중 같은 반경·같은 종류의 개수와 같은 줄을 파랗게 쓴다.
UCLASS()
class THATSMINE_API UMagnifierCountWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& geometry, float delta_time) override;

private:
	static constexpr float COUNT_RADIUS = 3000.0f;
	static constexpr float UPDATE_INTERVAL = 0.25f;
	static constexpr int32 TITLE_FONT_SIZE = 18;
	static constexpr int32 FONT_SIZE = 16;
	// 화면 오른쪽 끝에서 띄우는 거리
	static constexpr float SCREEN_MARGIN = 28.0f;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> list_box_;

	// 종류 한 줄씩. 필요한 만큼 만들어 두고 남는 줄은 접는다.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> line_texts_;

	FSlateFontInfo line_font_;

	double next_update_time_ = 0.0;
};
