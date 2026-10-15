#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WorldMapWidget.generated.h"

class UCanvasPanel;
class UMapViewWidget;

DECLARE_DELEGATE(FOnWorldMapCloseRequested);

UCLASS()
class THATSMINE_API UWorldMapWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UWorldMapWidget(const FObjectInitializer& object_initializer);
	FOnWorldMapCloseRequested on_close_requested_;

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

private:
	static constexpr double MAP_SIZE = 1000.0;
	static constexpr double MAX_ZOOM = 4.0;
	static constexpr double WHEEL_ZOOM_FACTOR = 1.2;
	static constexpr double ZOOM_SPEED = 18.0;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> map_canvas_;

	// 지도 사진·배제 그늘·격자·지명·표시를 그리는 게임 지도. 확대·이동한 범위를 보인다.
	UPROPERTY(Transient)
	TObjectPtr<UMapViewWidget> map_view_;

	double zoom_ = 1.0;
	double target_zoom_ = 1.0;
	FVector2D zoom_anchor_ = FVector2D::ZeroVector;
	FVector2D pan_ = FVector2D::ZeroVector;
	FVector2D drag_position_ = FVector2D::ZeroVector;
	bool is_dragging_ = false;

	void ChangeZoom(float wheel_delta, const FVector2D& anchor);
	void UpdateMap();

	void Close();
};
