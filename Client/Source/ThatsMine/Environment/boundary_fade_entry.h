#pragma once

#include "CoreMinimal.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

struct FBoundaryFadeEntry
{
	TWeakObjectPtr<UStaticMeshComponent> component;
	TWeakObjectPtr<UMaterialInstanceDynamic> material;
	FBox local_bounds;
	float opacity = 0.0f;
};
