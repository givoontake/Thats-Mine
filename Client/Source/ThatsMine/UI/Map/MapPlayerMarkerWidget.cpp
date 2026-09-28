#include "MapPlayerMarkerWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "Rendering/DrawElementTypes.h"

TSharedRef<SWidget> UMapPlayerMarkerWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		USizeBox* size = WidgetTree->ConstructWidget<USizeBox>();
		size->SetWidthOverride(40.0f);
		size->SetHeightOverride(40.0f);
		WidgetTree->RootWidget = size;
	}
	return Super::RebuildWidget();
}

int32 UMapPlayerMarkerWidget::NativePaint(const FPaintArgs& args, const FGeometry& geometry, const FSlateRect& culling_rect, FSlateWindowElementList& draw_elements, int32 layer_id, const FWidgetStyle& widget_style, bool is_parent_enabled) const
{
	const int32 layer = Super::NativePaint(args, geometry, culling_rect, draw_elements, layer_id, widget_style, is_parent_enabled);
	// 회전해도 끝점이 위젯 경계를 넘지 않도록 여백을 두고, 두 삼각형으로 면을 채운다.
	const FVector2f points[] = {FVector2f(20.0f, 6.0f), FVector2f(31.0f, 31.0f), FVector2f(20.0f, 25.0f), FVector2f(9.0f, 31.0f)};
	const FColor color(100, 205, 255, 255);
	TArray<FSlateVertex> vertices;
	vertices.Reserve(4);
	for (const FVector2f& point : points)
	{
		vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(geometry.GetAccumulatedRenderTransform(), point, FVector2f::ZeroVector, color));
	}
	const TArray<SlateIndex> indices = {0, 1, 2, 0, 2, 3};
	FSlateDrawElement::MakeCustomVerts(draw_elements, layer + 1, FSlateResourceHandle(), vertices, indices, nullptr, 0, 0);
	return layer + 1;
}
