#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PingMarkerWidget.generated.h"

class UCanvasPanel;
class UTextBlock;

// 게임 화면에 내가 찍은 핑 표시와 내 발밑에서 핑까지 거리(m, 높이 포함)를 띄운다.
// 핑이 화면 밖이나 카메라 뒤에 있으면 핑 쪽 방향의 화면 가장자리에 붙인다.
UCLASS()
class THATSMINE_API UPingMarkerWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& geometry, float delta_time) override;

private:
	static constexpr double ICON_SIZE = 18.0;
	static constexpr int32 FONT_SIZE = 16;
	// 화면 가장자리에 붙일 때 띄우는 거리
	static constexpr double SCREEN_MARGIN = 40.0;

	// 핑 표시와 거리 글씨를 담는 칸. 칸 가운데 위쪽이 핑 자리다.
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> marker_canvas_;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> distance_text_;
};
