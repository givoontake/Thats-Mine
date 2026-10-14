#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CoordinateWidget.generated.h"

class UTextBlock;

// 화면 왼쪽 위에 내 캐릭터의 월드 좌표 X·Y·Z를 m 단위로 표시한다.
UCLASS()
class THATSMINE_API UCoordinateWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& geometry, float delta_time) override;

private:
	static constexpr int32 FONT_SIZE = 18;
	// 화면 왼쪽 위 끝에서 띄우는 거리
	static constexpr float SCREEN_MARGIN = 28.0f;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> coordinate_text_;

	// 마지막으로 표시한 좌표(0.1m 단위 정수)
	FIntVector shown_decimeters_ = FIntVector(MAX_int32);
};
