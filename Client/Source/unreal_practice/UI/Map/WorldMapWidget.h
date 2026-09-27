#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "map_place.h"
#include "WorldMapWidget.generated.h"

class UCanvasPanel;
class UImage;
class UMapPlayerMarkerWidget;
class UTexture2D;
class UTextBlock;

DECLARE_DELEGATE(FOnWorldMapCloseRequested);

UCLASS()
class UNREAL_PRACTICE_API UWorldMapWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UWorldMapWidget(const FObjectInitializer& object_initializer);
	FOnWorldMapCloseRequested on_close_requested_;
	FOnWorldMapCloseRequested on_inventory_requested_;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& geometry, float delta_time) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& geometry, const FKeyEvent& event) override;
	virtual FReply NativeOnKeyDown(const FGeometry& geometry, const FKeyEvent& event) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& geometry, const FPointerEvent& event) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& geometry, const FPointerEvent& event) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& geometry, const FPointerEvent& event) override;
	virtual FReply NativeOnMouseMove(const FGeometry& geometry, const FPointerEvent& event) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& event) override;

	UPROPERTY(EditDefaultsOnly, Category = "Map")
	TSoftObjectPtr<UTexture2D> map_texture_ = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Thats_Mine/UI/Map/T_WorldMap.T_WorldMap")));

	UPROPERTY(EditDefaultsOnly, Category = "Map")
	TArray<FMapPlace> places_;

	// 지도 촬영 시 사용한 경계. 이미지 위쪽은 -X, 오른쪽은 -Y이다.
	UPROPERTY(EditDefaultsOnly, Category = "Map|Coordinates")
	FVector2D world_min_ = FVector2D(-100750.0, -100700.0);

	UPROPERTY(EditDefaultsOnly, Category = "Map|Coordinates")
	FVector2D world_max_ = FVector2D(100650.0, 100700.0);

private:
	static constexpr double MAP_SIZE = 1000.0;
	static constexpr double MAX_ZOOM = 4.0;
	static constexpr double WHEEL_ZOOM_FACTOR = 1.2;
	static constexpr double ZOOM_SPEED = 18.0;
	static constexpr int32 PLACE_FONT_SIZE = 22;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> map_canvas_;

	UPROPERTY(Transient)
	TObjectPtr<UImage> map_image_;

	UPROPERTY(Transient)
	TObjectPtr<UMapPlayerMarkerWidget> player_marker_;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> place_labels_;

	double zoom_ = 1.0;
	double target_zoom_ = 1.0;
	FVector2D zoom_anchor_ = FVector2D::ZeroVector;
	FVector2D pan_ = FVector2D::ZeroVector;
	FVector2D drag_position_ = FVector2D::ZeroVector;
	bool is_dragging_ = false;

	void ChangeZoom(float wheel_delta, const FVector2D& anchor);
	void UpdateMap();
	void UpdatePlayerMarker();

	void ResetView();

	void Close();
};
