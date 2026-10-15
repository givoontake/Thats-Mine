#include "StunGaugeWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElementTypes.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "UI/game_font.h"

TSharedRef<SWidget> UStunGaugeWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		USizeBox* size = WidgetTree->ConstructWidget<USizeBox>();
		size->SetWidthOverride(WIDGET_WIDTH);
		size->SetHeightOverride(WIDGET_HEIGHT);
		WidgetTree->RootWidget = size;
	}
	const auto load_icon = [](const TSoftObjectPtr<UTexture2D>& texture, FSlateBrush& brush)
	{
		if (UTexture2D* icon = texture.LoadSynchronous())
		{
			brush.SetResourceObject(icon);
			// 에디터에서는 텍스처가 컴파일 중이면 임시 크기가 나오므로 원본 크기로 비율을 정한다.
			brush.ImageSize = FVector2D(icon->GetImportedSize());
		}
	};
	load_icon(icon_texture_, icon_brush_);
	load_icon(protection_icon_texture_, protection_icon_brush_);
	SetVisibility(ESlateVisibility::HitTestInvisible);
	return Super::RebuildWidget();
}

void UStunGaugeWidget::SetStunProgress(float progress, bool animate)
{
	progress_ = FMath::Clamp(progress, 0.0f, 1.0f);
	if (!animate)
	{
		displayed_progress_ = progress_;
	}
}

void UStunGaugeWidget::SetProtectionSeconds(int32 seconds)
{
	protection_seconds_ = FMath::Max(seconds, 0);
}

void UStunGaugeWidget::NativeTick(const FGeometry& geometry, float delta_time)
{
	Super::NativeTick(geometry, delta_time);
	displayed_progress_ = FMath::FInterpConstantTo(displayed_progress_, progress_, delta_time, 2.5f);
}

int32 UStunGaugeWidget::NativePaint(const FPaintArgs& args, const FGeometry& geometry, const FSlateRect& culling_rect, FSlateWindowElementList& draw_elements, int32 layer_id, const FWidgetStyle& widget_style, bool is_parent_enabled) const
{
	int32 layer = Super::NativePaint(args, geometry, culling_rect, draw_elements, layer_id, widget_style, is_parent_enabled);
	// 양끝이 둥근 막대에 굵은 테두리를 두르고, 채움은 아랫부분을 진하게 깔고 윗부분에 반짝임을 얹는다.
	constexpr float BAR_WIDTH = 360.0f;
	constexpr float BAR_HEIGHT = 24.0f;
	constexpr float BORDER = 3.0f;
	constexpr float TRACK_HEIGHT = BAR_HEIGHT - BORDER * 2.0f;
	constexpr float SHADE_HEIGHT = 4.0f;
	constexpr float SHINE_HEIGHT = TRACK_HEIGHT * 0.3f;
	// 아이콘은 막대 왼쪽에 간격을 두고 그린다.
	constexpr float ICON_HEIGHT = 25.0f;
	constexpr float ICON_GAP = 8.0f;
	// 보호 중이면 방패 아이콘과 그 아래 남은 초를 한 묶음으로 세로 가운데에 둔다.
	constexpr float TIMER_HEIGHT = 15.0f;
	const bool is_protected = protection_seconds_ > 0;
	const FSlateBrush& icon_brush = is_protected ? protection_icon_brush_ : icon_brush_;
	const bool has_icon = icon_brush.GetResourceObject() && icon_brush.ImageSize.Y > 0.0;
	const float icon_width = has_icon ? ICON_HEIGHT * static_cast<float>(icon_brush.ImageSize.X / icon_brush.ImageSize.Y) : 0.0f;
	const float bar_offset = has_icon ? icon_width + ICON_GAP : 0.0f;
	const FVector2f size = FVector2f(geometry.GetLocalSize());
	const float content_left = (size.X - (bar_offset + BAR_WIDTH)) * 0.5f;
	const FVector2f origin(content_left + bar_offset, (size.Y - BAR_HEIGHT) * 0.5f);
	const FLinearColor tint = widget_style.GetColorAndOpacityTint();
	// 둥근 모서리 반지름은 브러시마다 고정이라 높이별로 따로 둔다.
	static const FSlateRoundedBoxBrush bar_brush(FLinearColor::White, BAR_HEIGHT * 0.5f);
	static const FSlateRoundedBoxBrush track_brush(FLinearColor::White, TRACK_HEIGHT * 0.5f);
	static const FSlateRoundedBoxBrush shine_brush(FLinearColor::White, SHINE_HEIGHT * 0.5f);
	const auto draw_box = [&](const FSlateBrush& brush, const FVector2f& offset, const FVector2f& box_size, const FLinearColor& color)
	{
		FSlateDrawElement::MakeBox(draw_elements, ++layer, geometry.ToPaintGeometry(box_size, FSlateLayoutTransform(offset)), &brush, ESlateDrawEffect::None, color * tint);
	};

	// 아래로 번지는 그림자를 크기를 조금씩 키운 반투명 막대 여러 장으로 흉내 낸다.
	for (int32 spread = 2; spread >= 0; --spread)
	{
		draw_box(bar_brush, origin + FVector2f(-spread, 3.0f - spread), FVector2f(BAR_WIDTH + spread * 2.0f, BAR_HEIGHT + spread * 2.0f), FLinearColor(0.0f, 0.0f, 0.0f, 0.16f));
	}
	draw_box(bar_brush, origin, FVector2f(BAR_WIDTH, BAR_HEIGHT), FLinearColor::FromSRGBColor(FColor(58, 36, 24, 255)));
	const FVector2f track_origin = origin + FVector2f(BORDER, BORDER);
	const float track_width = BAR_WIDTH - BORDER * 2.0f;
	draw_box(track_brush, track_origin, FVector2f(track_width, TRACK_HEIGHT), FLinearColor::FromSRGBColor(FColor(52, 40, 38, 255)));
	if (displayed_progress_ > 0.0f)
	{
		// 채움이 막대 높이보다 짧으면 둥근 끝이 겹치지 않도록 최소 길이로 그리고 투명도로 서서히 나타낸다.
		const float fill_width = track_width * displayed_progress_;
		const float drawn_width = FMath::Max(fill_width, TRACK_HEIGHT);
		const float appear = FMath::Clamp(fill_width / TRACK_HEIGHT, 0.0f, 1.0f);
		FLinearColor shade_color = fill_color_ * FLinearColor(0.6f, 0.45f, 0.3f, 1.0f);
		FLinearColor base_color = fill_color_;
		shade_color.A *= appear;
		base_color.A *= appear;
		draw_box(track_brush, track_origin, FVector2f(drawn_width, TRACK_HEIGHT), shade_color);
		draw_box(track_brush, track_origin, FVector2f(drawn_width, TRACK_HEIGHT - SHADE_HEIGHT), base_color);
		const float shine_inset = TRACK_HEIGHT * 0.35f;
		if (drawn_width > shine_inset * 2.0f + SHINE_HEIGHT)
		{
			draw_box(shine_brush, track_origin + FVector2f(shine_inset, 2.0f), FVector2f(drawn_width - shine_inset * 2.0f, SHINE_HEIGHT), FLinearColor(1.0f, 0.95f, 0.85f, 0.6f * appear));
		}
	}
	if (has_icon)
	{
		const float icon_top = (size.Y - ICON_HEIGHT - (is_protected ? TIMER_HEIGHT : 0.0f)) * 0.5f;
		draw_box(icon_brush, FVector2f(content_left, icon_top), FVector2f(icon_width, ICON_HEIGHT), FLinearColor::White);
		if (is_protected)
		{
			FSlateFontInfo font = GetGameFont(10);
			font.OutlineSettings.OutlineSize = 1;
			font.OutlineSettings.OutlineColor = FLinearColor::FromSRGBColor(FColor(16, 40, 90, 255));
			const FString text = FString::FromInt(protection_seconds_);
			const FVector2f text_size = FVector2f(FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(text, font));
			const FVector2f text_origin(content_left + (icon_width - text_size.X) * 0.5f, icon_top + ICON_HEIGHT + (TIMER_HEIGHT - text_size.Y) * 0.5f);
			FSlateDrawElement::MakeText(draw_elements, ++layer, geometry.ToPaintGeometry(text_size, FSlateLayoutTransform(text_origin)), text, font, ESlateDrawEffect::None, FLinearColor::White * tint);
		}
	}
	return layer;
}
