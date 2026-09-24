#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MapPlayerMarkerWidget.generated.h"

UCLASS()
class UNREAL_PRACTICE_API UMapPlayerMarkerWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual int32 NativePaint(const FPaintArgs& args, const FGeometry& geometry, const FSlateRect& culling_rect, FSlateWindowElementList& draw_elements, int32 layer_id, const FWidgetStyle& widget_style, bool is_parent_enabled) const override;
};
