#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MinimapWidget.generated.h"

class UMapViewWidget;

// 화면 오른쪽 위의 미니맵. 위가 항상 북쪽이고, 게임 지도(UMapViewWidget) 중 내 캐릭터를 가운데에 둔 둘레 VIEW_SIZE만큼을 보인다.
// 배제 그늘·격자·보물·핑·내 화살표는 큰 지도와 같은 지도가 그린다.
UCLASS()
class THATSMINE_API UMinimapWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 미니맵 창 한 변(테두리 포함)
	static constexpr double FRAME_SIZE = 226.0;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& geometry, float delta_time) override;

private:
	// 지도가 보이는 창 한 변과, 그 창에 담는 월드 너비(cm)
	static constexpr double VIEW_PIXELS = 220.0;
	static constexpr double VIEW_SIZE = 50000.0;
	static constexpr double BORDER = 3.0;
	static constexpr float MARKER_SCALE = 0.7f;

	UPROPERTY(Transient)
	TObjectPtr<UMapViewWidget> map_view_;
};
