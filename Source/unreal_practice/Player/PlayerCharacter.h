#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "weapon_settings.h"
#include "weapon_type.h"
#include "PlayerCharacter.generated.h"

class UCameraComponent;
class UInputAction;
class UUserWidget;
struct FInputActionValue;

UCLASS()
class UNREAL_PRACTICE_API APlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	APlayerCharacter();
	virtual void Jump() override;

	UFUNCTION(BlueprintCallable, Category = "Player|Weapon")
	void SelectWeapon(EWeaponType weapon);

	UFUNCTION(BlueprintCallable, Category = "Player|Actions")
	virtual void PrimaryAction();

	UFUNCTION(BlueprintCallable, Category = "Player|Actions")
	void PickUp();

	UFUNCTION(BlueprintCallable, Category = "Player|Scope")
	void ToggleAim();

	const FWeaponSettings* GetCurrentWeaponSettings() const;
	USceneComponent* GetEquippedWeaponMesh() const;
	bool IsAiming() const;

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

	uint32 action_id_ = 0;
	bool should_restore_movement_ = false;
	EMovementMode previous_movement_mode_ = MOVE_Walking;
	uint8 previous_custom_movement_mode_ = 0;

	void Move(const FInputActionValue& value);
	void Look(const FInputActionValue& value);
	void SelectWeaponFromKey(FKey key);
	void UpdateWeaponVisuals();
	void SetAiming(bool is_aiming);
	void PlayAction(const FActionSettings& action);
	void FinishAction(uint32 action_id);
};
