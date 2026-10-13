#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Styling/SlateTypes.h"

class UTexture2D;

// 맵을 여는 동안 엔진 로딩 화면 스레드에서도 그릴 수 있도록 Slate로 만든 로딩 화면.
// 배경 텍스처는 이 위젯을 만든 쪽이 살려 둔다. 팁이 있으면 아래쪽 띠 맨 위에 보인다.
class THATSMINE_API SLoadingScreenWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLoadingScreenWidget) {}
		SLATE_ARGUMENT(UTexture2D*, background)
		SLATE_ATTRIBUTE(float, progress)
		SLATE_ATTRIBUTE(FText, status)
		SLATE_ARGUMENT(FText, tip)
	SLATE_END_ARGS()

	void Construct(const FArguments& args);

private:
	FSlateBrush background_brush_;
	FSlateBrush band_brush_;
	FProgressBarStyle gauge_style_;
	TAttribute<float> progress_;
};
