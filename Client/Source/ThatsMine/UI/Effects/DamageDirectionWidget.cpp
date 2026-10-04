#include "DamageDirectionWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/SizeBox.h"
#include "Rendering/DrawElementTypes.h"

TSharedRef<SWidget> UDamageDirectionWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		WidgetTree->RootWidget = WidgetTree->ConstructWidget<USizeBox>();
	}
	return Super::RebuildWidget();
}

void UDamageDirectionWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Collapsed);
}

void UDamageDirectionWidget::ShowHit(const FVector& attack_direction, float camera_yaw)
{
	// 카메라 앞은 위쪽, 오른쪽은 시계 방향으로 표시한다.
	angle_ = FMath::DegreesToRadians(FMath::FindDeltaAngleDegrees(camera_yaw, attack_direction.Rotation().Yaw));
	elapsed_ = 0.0f;
	SetRenderOpacity(1.0f);
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UDamageDirectionWidget::NativeTick(const FGeometry& geometry, float delta_time)
{
	Super::NativeTick(geometry, delta_time);
	elapsed_ += delta_time;
	const float progress = FMath::Clamp(elapsed_ / duration_, 0.0f, 1.0f);
	// 처음 잠깐 유지한 뒤 부드럽게 사라진다. 재피격하면 표시 시간을 갱신한다.
	const float fade = FMath::Clamp((progress - 0.2f) / 0.8f, 0.0f, 1.0f);
	SetRenderOpacity(1.0f - fade * fade * (3.0f - 2.0f * fade));
	if (progress >= 1.0f)
	{
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

int32 UDamageDirectionWidget::NativePaint(const FPaintArgs& args, const FGeometry& geometry, const FSlateRect& culling_rect, FSlateWindowElementList& draw_elements, int32 layer_id, const FWidgetStyle& widget_style, bool is_parent_enabled) const
{
	const int32 layer = Super::NativePaint(args, geometry, culling_rect, draw_elements, layer_id, widget_style, is_parent_enabled);
	constexpr int32 SEGMENTS = 64;
	const FVector2D center = geometry.GetLocalSize() * 0.5;
	const float sweep = FMath::DegreesToRadians(arc_degrees_);
	// 일정한 두께의 호를 그리고 바깥쪽의 반투명 색을 안쪽으로 갈수록 투명하게 만든다.
	const float outer_radius = radius_ + thickness_ * 0.5f;
	const FLinearColor tint = color_ * widget_style.GetColorAndOpacityTint();
	TArray<FSlateVertex> vertices;
	TArray<SlateIndex> indices;
	vertices.Reserve((SEGMENTS + 1) * 4);
	indices.Reserve(SEGMENTS * 18);
	for (int32 index = 0; index <= SEGMENTS; ++index)
	{
		const float progress = static_cast<float>(index) / SEGMENTS;
		const float angle = angle_ - sweep * 0.5f + sweep * progress;
		const float feather = FMath::Min(0.5f, thickness_ * 0.25f);
		const float radii[] = {outer_radius - thickness_, outer_radius - (thickness_ + feather) * 0.5f, outer_radius - feather, outer_radius + feather};
		const float opacity[] = {0.0f, 0.5f, 1.0f, 0.0f};
		const FVector2D direction(FMath::Sin(angle), -FMath::Cos(angle));
		for (int32 band = 0; band < 4; ++band)
		{
			FLinearColor color = tint;
			color.A *= opacity[band];
			const FVector2f point(center + direction * radii[band]);
			vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(geometry.GetAccumulatedRenderTransform(), point, FVector2f::ZeroVector, color.ToFColor(true)));
		}
		if (index > 0)
		{
			for (int32 band = 0; band < 3; ++band)
			{
				const SlateIndex previous = (index - 1) * 4 + band;
				const SlateIndex current = index * 4 + band;
				indices.Append({previous, current, static_cast<SlateIndex>(previous + 1), static_cast<SlateIndex>(previous + 1), current, static_cast<SlateIndex>(current + 1)});
			}
		}
	}
	FSlateDrawElement::MakeCustomVerts(draw_elements, layer + 1, FSlateResourceHandle(), vertices, indices, nullptr, 0, 0);
	return layer + 1;
}
