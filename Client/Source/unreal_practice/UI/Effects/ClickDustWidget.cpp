#include "ClickDustWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Widgets/SWidget.h"
#include "GameFramework/PlayerController.h"

TSharedRef<SWidget> UClickDustWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		UCanvasPanel* canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = canvas;
		FSlateBrush brush;
		brush.SetResourceObject(texture_.LoadSynchronous());
		brush.ImageSize = FVector2f(PUFF_SIZE, PUFF_SIZE);
		particles_.SetNum(MAX_PARTICLES);
		for (FClickDustParticle& particle : particles_)
		{
			particle.image = WidgetTree->ConstructWidget<UImage>();
			particle.image->SetBrush(brush);
			particle.image->SetRenderOpacity(0.0f);
			particle.image->SetRenderTransformPivot(FVector2D(0.5, 0.5));
			UCanvasPanelSlot* slot = canvas->AddChildToCanvas(particle.image);
			slot->SetAlignment(FVector2D(0.5, 0.5));
			slot->SetSize(FVector2D(PUFF_SIZE, PUFF_SIZE));
		}
	}
	SetVisibility(ESlateVisibility::Collapsed);
	return Super::RebuildWidget();
}

void UClickDustWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ResetEffects();
}

void UClickDustWidget::PlayAtCursor()
{
	APlayerController* controller = GetOwningPlayer();
	float x = 0.0f;
	float y = 0.0f;
	if (!controller || !controller->bShowMouseCursor || !IsInViewport() || particles_.IsEmpty() || !UWidgetLayoutLibrary::GetMousePositionScaledByDPI(controller, x, y))
	{
		return;
	}
	const FVector2D directions[] = {FVector2D(-30.0, -14.0), FVector2D(0.0, -34.0), FVector2D(30.0, -10.0)};
	for (const FVector2D& direction : directions)
	{
		FClickDustParticle& particle = particles_[next_particle_];
		next_particle_ = (next_particle_ + 1) % particles_.Num();
		particle.position = FVector2D(x, y);
		particle.drift = direction;
		particle.elapsed = 0.0f;
		particle.is_active = true;
		CastChecked<UCanvasPanelSlot>(particle.image->Slot)->SetPosition(particle.position);
		particle.image->SetRenderScale(FVector2D(0.3, 0.3));
		particle.image->SetRenderOpacity(0.65f);
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (TSharedPtr<SWidget> widget = GetCachedWidget())
	{
		widget->SetCanTick(true);
	}
}

void UClickDustWidget::NativeTick(const FGeometry& geometry, float delta_time)
{
	Super::NativeTick(geometry, delta_time);
	bool has_active_particles = false;
	for (FClickDustParticle& particle : particles_)
	{
		if (!particle.is_active)
		{
			continue;
		}
		particle.elapsed += delta_time;
		const float progress = FMath::Clamp(particle.elapsed / FMath::Max(duration_, 0.1f), 0.0f, 1.0f);
		if (progress >= 1.0f)
		{
			particle.is_active = false;
			particle.image->SetRenderOpacity(0.0f);
			continue;
		}
		has_active_particles = true;
		const float spread = 1.0f - FMath::Square(1.0f - progress);
		const float scale = FMath::Lerp(0.3f, 1.05f, spread);
		CastChecked<UCanvasPanelSlot>(particle.image->Slot)->SetPosition(particle.position + particle.drift * spread);
		particle.image->SetRenderScale(FVector2D(scale, scale));
		particle.image->SetRenderOpacity(0.65f * FMath::Square(1.0f - progress));
	}
	if (!has_active_particles)
	{
		ResetEffects();
	}
}

void UClickDustWidget::ResetEffects()
{
	for (FClickDustParticle& particle : particles_)
	{
		particle.is_active = false;
		particle.image->SetRenderOpacity(0.0f);
	}
	next_particle_ = 0;
	SetVisibility(ESlateVisibility::Collapsed);
	if (TSharedPtr<SWidget> widget = GetCachedWidget())
	{
		widget->SetCanTick(false);
	}
}
