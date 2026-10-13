#include "LoadingScreenWidget.h"

#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

void SLoadingScreenWidget::Construct(const FArguments& args)
{
	progress_ = args._progress;
	background_brush_.SetResourceObject(args._background);
	// 에디터에서는 텍스처 준비가 끝나기 전 임시 크기가 나오므로, 비율은 가져올 때의 원본 크기로 정한다.
	background_brush_.ImageSize = args._background ? FVector2D(args._background->GetImportedSize()) : FVector2D(1920.0, 1080.0);
	band_brush_ = FSlateColorBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.45f));
	gauge_style_ = FProgressBarStyle()
		.SetBackgroundImage(FSlateRoundedBoxBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f), 5.0f))
		.SetFillImage(FSlateRoundedBoxBrush(FLinearColor(1.0f, 0.72f, 0.25f, 1.0f), 5.0f))
		.SetMarqueeImage(FSlateRoundedBoxBrush(FLinearColor(1.0f, 0.72f, 0.25f, 1.0f), 5.0f));
	const FSlateFontInfo status_font = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 22);
	const FSlateFontInfo percent_font = FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 26);
	const FSlateFontInfo tip_font = FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 20);

	ChildSlot
	[
		SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SScaleBox)
			.Stretch(EStretch::ScaleToFill)
			[
				SNew(SImage).Image(&background_brush_)
			]
		]
		// 배경이 밝아서 아래쪽에 어두운 띠를 깔고 그 위에 문구와 게이지를 둔다.
		+ SOverlay::Slot()
		.VAlign(VAlign_Bottom)
		[
			SNew(SBorder)
			.BorderImage(&band_brush_)
			.Padding(FMargin(80.0f, 24.0f, 80.0f, 40.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 16.0f)
				[
					SNew(STextBlock)
					.Text(FText::Format(NSLOCTEXT("LoadingScreen", "Tip", "TIP  {0}"), args._tip))
					.Visibility(args._tip.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible)
					.Font(tip_font)
					.ColorAndOpacity(FLinearColor(1.0f, 0.85f, 0.55f, 1.0f))
					.ShadowOffset(FVector2D(1.0, 1.0))
					.AutoWrapText(true)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 12.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Bottom)
					[
						SNew(STextBlock)
						.Text(args._status)
						.Font(status_font)
						.ColorAndOpacity(FLinearColor::White)
						.ShadowOffset(FVector2D(1.0, 1.0))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Bottom)
					[
						SNew(STextBlock)
						.Text_Lambda([this]() { return FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(FMath::Clamp(progress_.Get(), 0.0f, 1.0f) * 100.0f))); })
						.Font(percent_font)
						.ColorAndOpacity(FLinearColor(1.0f, 0.85f, 0.55f, 1.0f))
						.ShadowOffset(FVector2D(1.0, 1.0))
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.HeightOverride(14.0f)
					[
						SNew(SProgressBar)
						.Style(&gauge_style_)
						.Percent_Lambda([this]() { return TOptional<float>(FMath::Clamp(progress_.Get(), 0.0f, 1.0f)); })
					]
				]
			]
		]
	];
}
