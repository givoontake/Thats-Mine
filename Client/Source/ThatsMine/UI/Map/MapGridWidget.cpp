#include "MapGridWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Rendering/DrawElements.h"
#include "block_pieces.h"
#include "settings.h"

TSharedRef<SWidget> UMapGridWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		WidgetTree->RootWidget = WidgetTree->ConstructWidget<UCanvasPanel>();
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	return Super::RebuildWidget();
}

// 지도 사진은 가로가 y(왼쪽이 큰 쪽), 세로가 x(위가 작은 쪽) 방향이다. 축마다 칸 경계와 작은 격자선 자리를 위젯 좌표로 바꿔 그린다.
int32 UMapGridWidget::NativePaint(const FPaintArgs& args, const FGeometry& geometry, const FSlateRect& culling_rect, FSlateWindowElementList& draw_elements, int32 layer_id, const FWidgetStyle& widget_style, bool is_parent_enabled) const
{
	const int32 layer = Super::NativePaint(args, geometry, culling_rect, draw_elements, layer_id, widget_style, is_parent_enabled);
	const FVector2f size(geometry.GetLocalSize());
	const FVector2f world_size(MAP_MAX[1] - MAP_MIN[1], MAP_MAX[0] - MAP_MIN[0]);
	const FVector2f cell_size(BLOCK_CELL_SIZE_Y, BLOCK_CELL_SIZE_X);
	const FLinearColor tint = widget_style.GetColorAndOpacityTint();
	const FLinearColor line_color = FLinearColor(0.96f, 0.90f, 0.78f, 0.45f) * tint;
	const FLinearColor mini_line_color = FLinearColor(0.96f, 0.90f, 0.78f, 0.2f) * tint;
	for (int32 axis = 0; axis < 2; ++axis)
	{
		const float scale = size[axis] / world_size[axis];
		const float length = size[1 - axis];
		// 축 위치 position에 그 축과 수직인 선을 긋는다.
		const auto draw_line = [&](float position, int32 line_layer, const FLinearColor& color, float thickness)
		{
			const FVector2f start = axis == 0 ? FVector2f(position, 0.0f) : FVector2f(0.0f, position);
			const FVector2f end = axis == 0 ? FVector2f(position, length) : FVector2f(length, position);
			FSlateDrawElement::MakeLines(draw_elements, line_layer, geometry.ToPaintGeometry(), TArray<FVector2f>{ start, end }, ESlateDrawEffect::None, color, true, thickness);
		};
		for (int32 cell = 0; cell <= BLOCK_GRID_SIZE; ++cell)
		{
			const float cell_start = cell * cell_size[axis];
			draw_line(cell_start * scale, layer + 2, line_color, LINE_THICKNESS);
			if (cell == BLOCK_GRID_SIZE)
			{
				break;
			}
			for (float step = MINI_GRID_STEP; step < cell_size[axis] - MINI_GRID_STEP * 0.5f; step += MINI_GRID_STEP)
			{
				draw_line((cell_start + step) * scale, layer + 1, mini_line_color, MINI_LINE_THICKNESS);
			}
		}
	}
	return layer + 2;
}
