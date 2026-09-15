#pragma once

#include "CoreMinimal.h"
#include "click_dust_particle.generated.h"

class UImage;

USTRUCT()
struct FClickDustParticle
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UImage> image;
	FVector2D position = FVector2D::ZeroVector;
	FVector2D drift = FVector2D::ZeroVector;
	float elapsed = 0.0f;
	bool is_active = false;
};
