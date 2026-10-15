#include "MapViewWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/CanvasPanel.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/HintSubsystem.h"
#include "Interaction/PingSubsystem.h"
#include "Network/GameStartSubsystem.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "block_pieces.h"
#include "settings.h"
#include "UI/game_font.h"

FVector2D UMapViewWidget::GetMapPosition(const FVector& location)
{
	return FVector2D((MAP_MAX[1] - location.Y) / (MAP_MAX[1] - MAP_MIN[1]), (location.X - MAP_MIN[0]) / (MAP_MAX[0] - MAP_MIN[0]));
}

void UMapViewWidget::SetView(const FVector2D& view_min, const FVector2D& view_size)
{
	view_min_ = view_min;
	view_size_ = view_size;
}

TSharedRef<SWidget> UMapViewWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		WidgetTree->RootWidget = WidgetTree->ConstructWidget<UCanvasPanel>();
		map_texture_ = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Thats_Mine/UI/Map/T_WorldMap.T_WorldMap"));
		treasure_marker_texture_ = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Thats_Mine/UI/Map/T_TreasureMarker.T_TreasureMarker"));
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetClipping(EWidgetClipping::ClipToBounds);
	return Super::RebuildWidget();
}

// 지도 정규 좌표를 위젯 좌표로 바꿔 아래에서부터 사진, 배제 그늘, 격자, 지명, 보물, 핑, 화살표 순으로 그린다.
int32 UMapViewWidget::NativePaint(const FPaintArgs& args, const FGeometry& geometry, const FSlateRect& culling_rect, FSlateWindowElementList& draw_elements, int32 layer_id, const FWidgetStyle& widget_style, bool is_parent_enabled) const
{
	int32 layer = Super::NativePaint(args, geometry, culling_rect, draw_elements, layer_id, widget_style, is_parent_enabled);
	const FVector2D size = geometry.GetLocalSize();
	const FLinearColor tint = widget_style.GetColorAndOpacityTint();
	const auto to_pixel = [&](const FVector2D& map_position) { return (map_position - view_min_) / view_size_ * size; };
	const UWorld* world = GetWorld();

	// 사진과 배제 그늘은 보이는 범위 중 지도 안쪽만 그 부분의 텍스처 좌표로 그린다.
	const FVector2D visible_min(FMath::Max(view_min_.X, 0.0), FMath::Max(view_min_.Y, 0.0));
	const FVector2D visible_max(FMath::Min(view_min_.X + view_size_.X, 1.0), FMath::Min(view_min_.Y + view_size_.Y, 1.0));
	const auto draw_map_texture = [&](UTexture2D* texture, const FLinearColor& color)
	{
		if (!texture || visible_min.X >= visible_max.X || visible_min.Y >= visible_max.Y)
		{
			return;
		}
		FSlateBrush brush;
		brush.SetResourceObject(texture);
		brush.SetUVRegion(FBox2f(FVector2f(visible_min), FVector2f(visible_max)));
		const FVector2D pixel_min = to_pixel(visible_min);
		FSlateDrawElement::MakeBox(draw_elements, ++layer, geometry.ToPaintGeometry(FVector2f(to_pixel(visible_max) - pixel_min), FSlateLayoutTransform(FVector2f(pixel_min))), &brush, ESlateDrawEffect::None, color * tint);
	};
	draw_map_texture(map_texture_, FLinearColor::White);
	const UHintSubsystem* hints = world ? world->GetSubsystem<UHintSubsystem>() : nullptr;
	draw_map_texture(hints ? hints->GetExclusionTexture() : nullptr, FLinearColor(0.0f, 0.0f, 0.0f, EXCLUSION_SHADE_OPACITY));

	// 격자: 축마다 칸 경계를 굵게, 칸 경계에서 작은 격자 간격마다 가늘게 긋는다. 칸 끝의 작은 칸은 조금 넓다.
	++layer;
	const FVector2D world_size(MAP_MAX[1] - MAP_MIN[1], MAP_MAX[0] - MAP_MIN[0]);
	const FVector2D cell_size(BLOCK_CELL_SIZE_Y, BLOCK_CELL_SIZE_X);
	const FLinearColor line_color = FLinearColor(0.96f, 0.90f, 0.78f, 0.45f) * tint;
	const FLinearColor mini_line_color = FLinearColor(0.96f, 0.90f, 0.78f, 0.2f) * tint;
	const FVector2D map_min = to_pixel(FVector2D::ZeroVector);
	const FVector2D map_max = to_pixel(FVector2D(1.0, 1.0));
	for (int32 axis = 0; axis < 2; ++axis)
	{
		// 축 위치 world_position(지도 범위 시작에서 잰 cm)에 그 축과 수직인 선을 긋는다.
		const auto draw_line = [&](double world_position, int32 line_layer, const FLinearColor& color, float thickness)
		{
			const double position = map_min[axis] + world_position / world_size[axis] * (map_max[axis] - map_min[axis]);
			const FVector2f start = axis == 0 ? FVector2f(position, map_min.Y) : FVector2f(map_min.X, position);
			const FVector2f end = axis == 0 ? FVector2f(position, map_max.Y) : FVector2f(map_max.X, position);
			FSlateDrawElement::MakeLines(draw_elements, line_layer, geometry.ToPaintGeometry(), TArray<FVector2f>{ start, end }, ESlateDrawEffect::None, color, true, thickness);
		};
		for (int32 cell = 0; cell <= BLOCK_GRID_SIZE; ++cell)
		{
			const double cell_start = cell * cell_size[axis];
			draw_line(cell_start, layer + 1, line_color, LINE_THICKNESS);
			if (cell == BLOCK_GRID_SIZE)
			{
				break;
			}
			for (double step = MINI_GRID_STEP; step < cell_size[axis] - MINI_GRID_STEP * 0.5; step += MINI_GRID_STEP)
			{
				draw_line(cell_start + step, layer, mini_line_color, MINI_LINE_THICKNESS);
			}
		}
	}
	layer += 2;

	// 지명: 줄마다 가운데 맞춰, 아래쪽에 밝은 그림자를 깔고 그린다.
	const double place_scale = (map_max.X - map_min.X) / PLACE_MAP_PIXELS / PLACE_FONT_ZOOM;
	const TSharedRef<FSlateFontMeasure> font_measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	// 지명을 그리지 않는 지도는 빈 목록을 돈다.
	static const TArray<FMapPlace> NO_PLACES;
	for (const FMapPlace& place : is_places_visible_ ? GetMapPlaces() : NO_PLACES)
	{
		const FSlateFontInfo font = GetGameFont(FMath::RoundToInt32(place.font_size * PLACE_FONT_ZOOM));
		TArray<FString> lines;
		place.name.ToString().ParseIntoArrayLines(lines);
		const double line_height = font_measure->GetMaxCharacterHeight(font);
		const FVector2D center = to_pixel(place.position);
		for (int32 index = 0; index < lines.Num(); ++index)
		{
			const FVector2D text_size = font_measure->Measure(lines[index], font);
			const FVector2D offset = center + (FVector2D(-text_size.X * 0.5, (index - lines.Num() * 0.5) * line_height)) * place_scale;
			for (int32 pass = 0; pass < 2; ++pass)
			{
				const FVector2D shadow = pass == 0 ? FVector2D(0.0, PLACE_FONT_ZOOM * place_scale) : FVector2D::ZeroVector;
				const FLinearColor color = pass == 0 ? FLinearColor(1.0f, 0.96f, 0.83f, 0.38f) : FLinearColor(0.035f, 0.02f, 0.012f, 0.65f);
				FSlateDrawElement::MakeText(draw_elements, layer + pass, geometry.ToPaintGeometry(FVector2f(text_size), FSlateLayoutTransform(static_cast<float>(place_scale), FVector2f(offset + shadow))), lines[index], font, ESlateDrawEffect::None, color * tint);
			}
		}
	}
	layer += 2;

	// 보물 별: 누군가 발견했거나 시간이 끝나 공개된 보물과, 시험용으로 받은 보물 위치
	const UGameStartSubsystem* game_start = world ? world->GetSubsystem<UGameStartSubsystem>() : nullptr;
	FSlateBrush treasure_brush;
	treasure_brush.SetResourceObject(treasure_marker_texture_);
	const float treasure_size = TREASURE_MARKER_SIZE * marker_scale_;
	for (const TOptional<FVector>& treasure : { game_start ? game_start->GetTreasureLocation() : TOptional<FVector>(), game_start ? game_start->GetRevealedTreasureLocation() : TOptional<FVector>() })
	{
		if (treasure)
		{
			FSlateDrawElement::MakeBox(draw_elements, layer, geometry.ToPaintGeometry(FVector2f(treasure_size), FSlateLayoutTransform(FVector2f(to_pixel(GetMapPosition(*treasure))) - FVector2f(treasure_size * 0.5f))), &treasure_brush, ESlateDrawEffect::None, tint);
		}
	}
	++layer;

	// 내가 찍은 핑. 직접 그릴 때는 둥근 상자의 채움색을 그리기 색에 곱해야 한다.
	const UPingSubsystem* pings = world ? world->GetSubsystem<UPingSubsystem>() : nullptr;
	if (pings && pings->GetPingLocation())
	{
		const float ping_size = PING_MARKER_SIZE * marker_scale_;
		const FSlateRoundedBoxBrush ping_brush(FLinearColor(1.0f, 0.82f, 0.1f), ping_size * 0.5f, FLinearColor::White, 2.0f * marker_scale_, FVector2f(ping_size));
		FSlateDrawElement::MakeBox(draw_elements, layer, geometry.ToPaintGeometry(FVector2f(ping_size), FSlateLayoutTransform(FVector2f(to_pixel(GetMapPosition(*pings->GetPingLocation()))) - FVector2f(ping_size * 0.5f))), &ping_brush, ESlateDrawEffect::None, ping_brush.GetTint(widget_style) * tint);
	}
	++layer;

	// 내 화살표: 캐릭터가 보는 쪽으로 돌린 사각 화살촉을 두 삼각형으로 채운다.
	const APlayerController* controller = world ? world->GetFirstPlayerController() : nullptr;
	if (const APawn* pawn = controller ? controller->GetPawn() : nullptr)
	{
		const FVector forward = pawn->GetActorForwardVector();
		const float angle = FMath::Atan2(-forward.Y, -forward.X);
		const FVector2f center(to_pixel(GetMapPosition(pawn->GetActorLocation())));
		const float scale = PLAYER_MARKER_SIZE / 40.0f * marker_scale_;
		const FVector2f points[] = { FVector2f(0.0f, -14.0f), FVector2f(11.0f, 11.0f), FVector2f(0.0f, 5.0f), FVector2f(-11.0f, 11.0f) };
		TArray<FSlateVertex> vertices;
		vertices.Reserve(UE_ARRAY_COUNT(points));
		for (const FVector2f& point : points)
		{
			const FVector2f rotated(point.X * FMath::Cos(angle) - point.Y * FMath::Sin(angle), point.X * FMath::Sin(angle) + point.Y * FMath::Cos(angle));
			vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(geometry.GetAccumulatedRenderTransform(), center + rotated * scale, FVector2f::ZeroVector, (FLinearColor(FColor(100, 205, 255)) * tint).ToFColor(true)));
		}
		const TArray<SlateIndex> indices = { 0, 1, 2, 0, 2, 3 };
		FSlateDrawElement::MakeCustomVerts(draw_elements, layer, FSlateResourceHandle(), vertices, indices, nullptr, 0, 0);
	}
	return layer;
}
