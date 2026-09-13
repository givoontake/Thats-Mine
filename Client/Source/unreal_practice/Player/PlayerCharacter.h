#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "weapon_settings.h"
#include "weapon_type.h"
#include "shot_result.h"
#include "weapon_ammo_state.h"
#include "equipment_display.h"
#include "PlayerCharacter.generated.h"

class UCameraComponent;
class UEquipmentWidget;
class UInputAction;
class UUserWidget;
struct FInputActionValue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMeleeHit, const FHitResult&, hit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnShotFired, const FShotResult&, shot);

UCLASS()
class UNREAL_PRACTICE_API APlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	APlayerCharacter();
	virtual void Jump() override;
	virtual void PawnClientRestart() override;
	virtual void UnPossessed() override;

	UFUNCTION(BlueprintPure, Category = "Player|Weapon")
	FEquipmentDisplay GetEquipmentDisplay() const;

	UFUNCTION(BlueprintCallable, Category = "Player|Weapon")
	void SelectWeapon(EWeaponType weapon);

	UFUNCTION(BlueprintCallable, Category = "Player|Actions")
	virtual void PrimaryAction();

	UFUNCTION(BlueprintCallable, Category = "Player|Actions")
	void PickUp();

	UFUNCTION(BlueprintCallable, Category = "Player|Weapon")
	void Reload();

	UFUNCTION(BlueprintCallable, Category = "Player|Scope")
	void ToggleAim();

	const FWeaponSettings* GetCurrentWeaponSettings() const;
	USceneComponent* GetEquippedWeaponMesh() const;
	bool IsAiming() const;

	void ProcessMeleeHit(int32 montage_instance_id);

	UPROPERTY(BlueprintAssignable, Category = "Player|Combat")
	FOnMeleeHit on_melee_hit_;

	UPROPERTY(BlueprintAssignable, Category = "Player|Combat")
	FOnShotFired on_shot_fired_;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type end_play_reason) override;
	virtual void PostInitializeComponents() override;
	virtual void SetupPlayerInputComponent(UInputComponent* player_input_component) override;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> move_action_;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> mouse_look_action_;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Movement", meta = (ClampMin = "0"))
	float forward_speed_ = 600.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Movement", meta = (ClampMin = "0"))
	float backward_speed_ = 200.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Weapon")
	TMap<EWeaponType, FWeaponSettings> weapon_settings_;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Actions")
	FActionSettings pickup_action_;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera", meta = (UseComponentPicker))
	FComponentReference first_person_camera_reference_;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera", meta = (UseComponentPicker))
	FComponentReference third_person_camera_reference_;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Scope")
	TSubclassOf<UUserWidget> scope_widget_class_;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Equipment")
	TSubclassOf<UEquipmentWidget> equipment_widget_class_;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Scope", meta = (ClampMin = "5", ClampMax = "170"))
	float normal_field_of_view_ = 90.0f;

private:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Player|State", meta = (AllowPrivateAccess = "true"))
	EWeaponType current_weapon_ = EWeaponType::PICKAXE;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Player|State", meta = (AllowPrivateAccess = "true"))
	bool is_aiming_ = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Player|State", meta = (AllowPrivateAccess = "true"))
	bool is_action_locked_ = false;

	UPROPERTY(Transient)
	TMap<EWeaponType, TObjectPtr<USceneComponent>> weapon_meshes_;

	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> first_person_camera_;

	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> third_person_camera_;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> scope_widget_;

	UPROPERTY(Transient)
	TObjectPtr<UEquipmentWidget> equipment_widget_;

	TMap<EWeaponType, FWeaponAmmoState> weapon_ammo_;
	TMap<EWeaponType, double> next_fire_times_;

	uint32 action_id_ = 0;
	int32 active_montage_instance_id_ = INDEX_NONE;
	bool has_pending_melee_hit_ = false;
	FMeleeHitSettings active_melee_settings_;
	bool should_restore_movement_ = false;
	EMovementMode previous_movement_mode_ = MOVE_Walking;
	uint8 previous_custom_movement_mode_ = 0;

	void Move(const FInputActionValue& value);
	void Look(const FInputActionValue& value);
	void SelectWeaponFromKey(FKey key);
	void UpdateWeaponVisuals();
	void SetAiming(bool is_aiming);
	void PlayAction(const FActionSettings& action, const FMeleeHitSettings* melee_hit = nullptr);
	void FinishAction(uint32 action_id);
	void FireHitscan(const FHitscanSettings& settings);
	void BeginReload(EWeaponType weapon);
	void FinishReload(EWeaponType weapon);
	void PlayReloadAnimation(EWeaponType weapon);
	void UpdateEquipmentDisplay();
};
