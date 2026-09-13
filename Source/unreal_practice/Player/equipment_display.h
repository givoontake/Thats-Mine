#pragma once

#include "CoreMinimal.h"
#include "equipment_display.generated.h"

USTRUCT(BlueprintType)
struct FEquipmentDisplay
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	FText name;

	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	bool uses_ammo = false;

	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	bool should_show_crosshair = false;

	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	int32 rounds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	int32 capacity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	FText total_ammo;

	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	bool is_reloading = false;
};
