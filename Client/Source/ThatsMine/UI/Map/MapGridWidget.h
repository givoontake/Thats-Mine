#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MapGridWidget.generated.h"

// 지도 위에 그리는 격자. 위젯 범위를 지도 범위(MAP_MIN~MAP_MAX)로 보고, 배제 판(block_pieces.h)과 같은 8×8 칸 선을 굵게,
// 칸마다 칸 경계에서 MINI_GRID_STEP 간격의 작은 격자선을 가늘게 그린다. 칸 너비가 간격으로 나누어떨어지지 않아 칸 끝의 작은 칸은 조금 넓다.
// 큰 지도와 미니맵이 지도 사진을 덮는 자리에 같은 크기로 놓는다.
UCLASS()
class THATSMINE_API UMapGridWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual int32 NativePaint(const FPaintArgs& args, const FGeometry& geometry, const FSlateRect& culling_rect, FSlateWindowElementList& draw_elements, int32 layer_id, const FWidgetStyle& widget_style, bool is_parent_enabled) const override;

private:
	static constexpr float MINI_GRID_STEP = 5000.0f; // 작은 격자 간격(cm)
	static constexpr float LINE_THICKNESS = 1.5f;
	static constexpr float MINI_LINE_THICKNESS = 1.0f;
};
