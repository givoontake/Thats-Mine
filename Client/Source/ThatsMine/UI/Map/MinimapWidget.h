#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MinimapWidget.generated.h"

class UCanvasPanel;
class UImage;
class UMapGridWidget;
class UMapPlayerMarkerWidget;

// 화면 오른쪽 위의 미니맵. 위가 항상 북쪽이고, 내 캐릭터를 가운데에 두고 둘레 VIEW_SIZE만큼의 지도 사진을 보인다.
// 큰 지도와 같이 배제된 곳을 어둡게 덮고 배제 판과 같은 8×8 격자선과 50m 작은 격자선을 그린다. 내 화살표는 캐릭터가 보는 쪽으로 돌린다.
// 내가 찍은 핑과, 시험용으로 받은 보물 위치가 있으면 그 자리에 표시한다.
UCLASS()
class THATSMINE_API UMinimapWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 미니맵 창 한 변(테두리 포함)
	static constexpr double FRAME_SIZE = 226.0;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& geometry, float delta_time) override;

private:
	// 지도가 보이는 창 한 변과, 그 창에 담는 월드 너비(cm)
	static constexpr double VIEW_PIXELS = 220.0;
	static constexpr double VIEW_SIZE = 50000.0;
	static constexpr double BORDER = 3.0;
	static constexpr double MARKER_SCALE = 0.7;
	static constexpr double PING_MARKER_SIZE = 12.0;
	static constexpr double TREASURE_MARKER_SIZE = 24.0;
	static constexpr float EXCLUSION_SHADE_OPACITY = 0.3f;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> view_canvas_;

	UPROPERTY(Transient)
	TObjectPtr<UImage> map_image_;

	UPROPERTY(Transient)
	TObjectPtr<UImage> exclusion_image_;

	UPROPERTY(Transient)
	TObjectPtr<UMapGridWidget> grid_;

	UPROPERTY(Transient)
	TObjectPtr<UImage> treasure_marker_;

	UPROPERTY(Transient)
	TObjectPtr<UImage> ping_marker_;

	UPROPERTY(Transient)
	TObjectPtr<UMapPlayerMarkerWidget> player_marker_;

	// 힌트 서브시스템의 배제 텍스처를 다시 가져온다.
	void UpdateExclusionTexture();
	// location이 있으면 marker를 지도 원점 map_origin, 지도 크기 map_pixels 기준의 그 자리에 보이고, 없으면 숨긴다.
	static void PlaceMarker(UImage* marker, const TOptional<FVector>& location, const FVector2D& map_origin, const FVector2D& map_pixels);
};
