#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "melee_hit_settings.generated.h"

USTRUCT(BlueprintType)
struct FMeleeHitSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee")
	bool is_enabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee", meta = (EditCondition = "is_enabled", ClampMin = "0.0", ClampMax = "150.0", Units = "cm"))
	float distance = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee", meta = (EditCondition = "is_enabled"))
	TEnumAsByte<ECollisionChannel> trace_channel = ECC_Visibility;
};
