#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "weapon_settings.h"
#include "weapon_type.h"
#include "shot_result.h"
#include "weapon_ammo_state.h"
#include "equipment_display.h"
#include "inventory_hint.h"
#include "player_action.h"
#include "PlayerCharacter.generated.h"

class UCameraComponent;
class UEquipmentWidget;
class UInputAction;
class UUserWidget;
struct FInputActionValue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMeleeHit, const FHitResult&, hit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnShotFired, const FShotResult&, shot);

DECLARE_MULTICAST_DELEGATE(FOnEquipmentChanged);
DECLARE_MULTICAST_DELEGATE(FOnHintsChanged);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnActionStarted, EPlayerAction, EWeaponType);

UCLASS()
class THATSMINE_API APlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	APlayerCharacter();
	virtual void PawnClientRestart() override;
	virtual void UnPossessed() override;
	virtual FRotator GetViewRotation() const override;

	// 이동 입력(X 오른쪽, Y 앞쪽)을 이동 계산에 넣는다. 다른 플레이어는 받은 입력으로 호출한다.
	void ApplyMovementInput(const FVector2D& movement);

	// 다른 플레이어의 동작을 판정 없이 애니메이션만 재생한다.
	void PlayRemoteAction(EPlayerAction action, EWeaponType weapon);

	// 내 캐릭터가 동작을 시작했을 때 알린다.
	FOnActionStarted on_action_started_;

	UFUNCTION(BlueprintPure, Category = "Player|Weapon")
	FEquipmentDisplay GetEquipmentDisplay() const;

	FEquipmentDisplay GetWeaponDisplay(EWeaponType weapon) const;
	EWeaponType GetCurrentWeapon() const { return current_weapon_; }
	int32 GetPickaxeCharges() const { return pickaxe_charges_; }
	const TArray<FInventoryHint>& GetAcquiredHints() const { return acquired_hints_; }

	UFUNCTION(BlueprintCallable, Category = "Player|Inventory")
	void AddHint(const FInventoryHint& hint);

	void MarkHintsRead();
	FOnEquipmentChanged on_equipment_changed_;
	FOnHintsChanged on_hints_changed_;

	UFUNCTION(BlueprintCallable, Category = "Player|Weapon")
	void SelectWeapon(EWeaponType weapon);

	UFUNCTION(BlueprintCallable, Category = "Player|Actions")
	virtual void PrimaryAction();

	UFUNCTION(BlueprintCallable, Category = "Player|Actions")
	void PickUp();

	UFUNCTION(BlueprintCallable, Category = "Player|Actions")
	void Interact();

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
	virtual void OnMovementModeChanged(EMovementMode previous_movement_mode, uint8 previous_custom_mode) override;
	virtual void SetupPlayerInputComponent(UInputComponent* player_input_component) override;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> move_action_;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Input")
	TObjectPtr<UInputAction> mouse_look_action_;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Movement", meta = (ClampMin = "0"))
	float forward_speed_ = 600.0f;

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

	UPROPERTY(Transient)
	TArray<FInventoryHint> acquired_hints_;

	int32 pickaxe_charges_ = 3;

	bool is_equipping_ = false;
	uint32 equip_id_ = 0;
	uint32 action_id_ = 0;
	int32 active_montage_instance_id_ = INDEX_NONE;
	bool has_pending_melee_hit_ = false;
	FMeleeHitSettings active_melee_settings_;

	void Move(const FInputActionValue& value);
	void Look(const FInputActionValue& value);
	void SelectWeaponFromKey(FKey key);
	void UpdateWeaponVisuals();
	void SetAiming(bool is_aiming);
	bool PlayAction(const FActionSettings& action, const FMeleeHitSettings* melee_hit = nullptr);
	void FinishAction(uint32 action_id);
	void FireHitscan(const FHitscanSettings& settings);
	void PlayMuzzleEffect(const FHitscanSettings& settings);
	void BeginReload(EWeaponType weapon);
	void FinishReload(EWeaponType weapon);
	void PlayReloadAnimation(EWeaponType weapon);
	void UpdateEquipmentDisplay();
};
