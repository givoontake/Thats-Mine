#include "HammerCursorWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"
#include "Widgets/SWidget.h"

TSharedRef<SWidget> UHammerCursorWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		USizeBox* size = WidgetTree->ConstructWidget<USizeBox>();
		size->SetWidthOverride(size_);
		size->SetHeightOverride(size_);
		WidgetTree->RootWidget = size;
		UImage* image = WidgetTree->ConstructWidget<UImage>();
		FSlateBrush brush;
		brush.SetResourceObject(texture_.LoadSynchronous());
		brush.ImageSize = FVector2f(size_, size_);
		image->SetBrush(brush);
		// Slate centers software cursors; move the striking face onto that center.
		image->SetRenderTranslation((FVector2D(0.5, 0.5) - hotspot_) * size_);
		size->AddChild(image);
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	return Super::RebuildWidget();
}

void UHammerCursorWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (TSharedPtr<SWidget> widget = GetCachedWidget())
	{
		widget->SetCanTick(false);
	}
}
