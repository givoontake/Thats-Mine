#pragma once

#include "CoreMinimal.h"
#include "weapon_type.generated.h"

UENUM(BlueprintType)
enum class EWeaponType : uint8
{
	PICKAXE UMETA(DisplayName = "Pickaxe"),
	SNIPER UMETA(DisplayName = "Sniper")
};
