#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "hitscan_settings.generated.h"

class UAnimMontage;

USTRUCT(BlueprintType)
struct FHitscanSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitscan")
	bool is_enabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitscan", meta = (EditCondition = "is_enabled", ClampMin = "1", Units = "cm"))
	float distance = 10000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitscan", meta = (EditCondition = "is_enabled", ClampMin = "0.01", Units = "s"))
	float fire_interval = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitscan", meta = (EditCondition = "is_enabled", ClampMin = "1"))
	int32 magazine_capacity = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitscan", meta = (EditCondition = "is_enabled", ClampMin = "0.01", Units = "s"))
	float reload_duration = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitscan", meta = (EditCondition = "is_enabled"))
	TObjectPtr<UAnimMontage> reload_montage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitscan", meta = (EditCondition = "is_enabled"))
	FName muzzle_socket = TEXT("Muzzle");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitscan", meta = (EditCondition = "is_enabled", Units = "cm"))
	float body_height = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitscan", meta = (EditCondition = "is_enabled"))
	TEnumAsByte<ECollisionChannel> trace_channel = ECC_Visibility;
};
