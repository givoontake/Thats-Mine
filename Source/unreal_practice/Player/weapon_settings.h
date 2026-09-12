#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "InputCoreTypes.h"
#include "action_settings.h"
#include "weapon_settings.generated.h"

USTRUCT(BlueprintType)
struct FWeaponSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FKey selection_key;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (UseComponentPicker))
	FComponentReference mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FActionSettings primary_action;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	bool can_aim = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	bool uses_rifle_pose = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FName left_hand_grip_socket;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (EditCondition = "can_aim", ClampMin = "5", ClampMax = "170"))
	float aiming_field_of_view = 40.0f;
};
