#include "HintGradeBadge.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Materials/MaterialInterface.h"
#include "Styling/CoreStyle.h"
#include "UI/game_font.h"

UHintGradeBadge::UHintGradeBadge(const FObjectInitializer& object_initializer)
	: Super(object_initializer)
{
	grade_text_materials_ = {
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Thats_Mine/UI/Inventory/MI_GradeSText.MI_GradeSText"))),
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Thats_Mine/UI/Inventory/MI_GradeAText.MI_GradeAText")))
	};
	grade_border_materials_ = {
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Thats_Mine/UI/Inventory/MI_GradeSBorder.MI_GradeSBorder"))),
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Thats_Mine/UI/Inventory/MI_GradeABorder.MI_GradeABorder")))
	};
}

TSharedRef<SWidget> UHintGradeBadge::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		// 재질로 그리는 등급은 색을 곱하지 않는다.
		const int32 grade_index = static_cast<int32>(grade_);
		const bool is_gradient = grade_text_materials_.IsValidIndex(grade_index);
		const FLinearColor grade_color = is_gradient ? FLinearColor::White
			: grade_ == EHintGrade::B ? FLinearColor::FromSRGBColor(FColor(0, 141, 86)) : FLinearColor::FromSRGBColor(FColor(140, 151, 156));
		USizeBox* badge_size = WidgetTree->ConstructWidget<USizeBox>();
		badge_size->SetWidthOverride(42.0f);
		badge_size->SetHeightOverride(42.0f);
		WidgetTree->RootWidget = badge_size;
		UBorder* badge = WidgetTree->ConstructWidget<UBorder>();
		UMaterialInterface* border_material = grade_border_materials_.IsValidIndex(grade_index) ? grade_border_materials_[grade_index].LoadSynchronous() : nullptr;
		if (border_material)
		{
			FSlateBrush border_brush;
			border_brush.SetResourceObject(border_material);
			badge->SetBrush(border_brush);
		}
		else
		{
			badge->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.035f, 0.027f, 0.020f), 3.0f, grade_color, 2.5f));
		}
		badge->SetHorizontalAlignment(HAlign_Center);
		badge->SetVerticalAlignment(VAlign_Center);
		badge_size->AddChild(badge);
		UTextBlock* grade = WidgetTree->ConstructWidget<UTextBlock>();
		FSlateFontInfo grade_font = GetGameFont(26);
		if (is_gradient)
		{
			grade_font.FontMaterial = grade_text_materials_[grade_index].LoadSynchronous();
		}
		grade->SetFont(grade_font);
		grade->SetColorAndOpacity(grade_color);
		grade->SetText(FText::AsCultureInvariant(grade_ == EHintGrade::S ? TEXT("S") : grade_ == EHintGrade::A ? TEXT("A") : grade_ == EHintGrade::B ? TEXT("B") : TEXT("C")));
		// 게임 글꼴의 영문 글자는 글자 칸 가운데에 두어도 아래로 치우쳐 보여 칸 가운데로 올린다.
		grade->SetRenderTranslation(FVector2D(0.0, -GLYPH_RAISE));
		badge->AddChild(grade);
	}
	return Super::RebuildWidget();
}
