#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "map_place.h"
#include "MapViewWidget.generated.h"

class UTexture2D;

// 게임 지도 하나. 지도 사진, 힌트로 배제한 곳 그늘, 배제 판(block_pieces.h)과 같은 8×8 칸 격자와 MINI_GRID_STEP 간격의 작은 격자, 지명,
// 표시(발견하거나 시험용으로 공개된 보물, 내 핑, 내 화살표)를 그린다. 표시는 그릴 때마다 서브시스템에서 읽어, 이 지도를 쓰는 어느 화면에나 같은 것이 보인다.
// 큰 지도·미니맵·블록 판은 지도 정규 좌표(0~1) 중 보이는 범위만 정하고, 그 범위를 위젯 크기에 맞춰 그린다.
// 지도 사진은 가로가 y(왼쪽이 큰 쪽), 세로가 x(위가 작은 쪽) 방향이고 지도 범위(MAP_MIN~MAP_MAX)를 담는다.
UCLASS()
class THATSMINE_API UMapViewWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 월드 위치의 지도 정규 좌표
	static FVector2D GetMapPosition(const FVector& location);
	void SetView(const FVector2D& view_min, const FVector2D& view_size);
	// 표시 크기 배율. 큰 지도는 1이다.
	void SetMarkerScale(float marker_scale) { marker_scale_ = marker_scale; }
	// 지명(GetMapPlaces)을 그릴지
	void SetPlacesVisible(bool is_places_visible) { is_places_visible_ = is_places_visible; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual int32 NativePaint(const FPaintArgs& args, const FGeometry& geometry, const FSlateRect& culling_rect, FSlateWindowElementList& draw_elements, int32 layer_id, const FWidgetStyle& widget_style, bool is_parent_enabled) const override;

private:
	static constexpr float EXCLUSION_SHADE_OPACITY = 0.6f;
	static constexpr float MINI_GRID_STEP = 5000.0f; // 작은 격자 간격(cm)
	static constexpr float LINE_THICKNESS = 1.5f;
	static constexpr float MINI_LINE_THICKNESS = 1.0f;
	static constexpr float TREASURE_MARKER_SIZE = 36.0f;
	static constexpr float PING_MARKER_SIZE = 14.0f;
	static constexpr float PLAYER_MARKER_SIZE = 40.0f;
	// 지명 글자 크기(FMapPlace::font_size)는 지도 한 변이 이 픽셀일 때의 크기다.
	static constexpr float PLACE_MAP_PIXELS = 1000.0f;
	// 지명 글자는 이 배율의 크기로 만들어 줄여 그려, 확대할 때 정수 글꼴 크기가 바뀌며 튀지 않게 한다.
	static constexpr float PLACE_FONT_ZOOM = 4.0f;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> map_texture_;

	// 보물 자리에 놓는 주황 별
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> treasure_marker_texture_;

	bool is_places_visible_ = false;
	FVector2D view_min_ = FVector2D::ZeroVector;
	FVector2D view_size_ = FVector2D(1.0, 1.0);
	float marker_scale_ = 1.0f;
};
