#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "InputCoreTypes.h"
#include "action_settings.h"
#include "melee_hit_settings.h"
#include "hitscan_settings.h"
#include "weapon_settings.generated.h"

USTRUCT(BlueprintType)
struct FWeaponSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FText display_name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FKey selection_key;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (UseComponentPicker))
	FComponentReference mesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FActionSettings primary_action;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UAnimMontage> equip_montage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FMeleeHitSettings melee_hit;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FHitscanSettings hitscan;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	bool can_aim = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	bool uses_rifle_pose = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	bool uses_pistol_pose = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Pose", meta = (EditCondition = "uses_rifle_pose || uses_pistol_pose", ClampMin = "0", ClampMax = "89", Units = "deg"))
	float aim_pitch_up_limit = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Pose", meta = (EditCondition = "uses_rifle_pose || uses_pistol_pose", ClampMin = "0", ClampMax = "89", Units = "deg"))
	float aim_pitch_down_limit = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Pose", meta = (EditCondition = "uses_rifle_pose || uses_pistol_pose", ClampMin = "0.1"))
	float aim_pitch_interp_speed = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FName left_hand_grip_socket;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (EditCondition = "can_aim", ClampMin = "5", ClampMax = "170"))
	float aiming_field_of_view = 40.0f;
};
