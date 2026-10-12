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
class UDamageDirectionWidget;
class UEquipmentWidget;
class UInputAction;
class UNiagaraComponent;
class UNiagaraSystem;
class UPhysicsAsset;
class UStunGaugeWidget;
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

	// 서버가 보낸 내 기절 게이지 값을 표시하고, 이후 초당 감소량만큼 줄여 표시한다.
	void SetStunGauge(float gauge);
	// 서버가 아는 탄약 상태로 맞춘다. 들고 있는 총의 탄창이 비었을 때 소지 탄이 생기면 재장전한다.
	void SetAmmo(EWeaponType weapon, int32 rounds, int32 spare_rounds);
	// 서버가 아는 곡괭이 사용 가능 횟수와 다음 1번이 찰 때까지 남은 시간으로 맞춘다.
	void SetPickaxeCharges(int32 charges, int32 remaining_ms);
	// 기절을 시작한다. 별 이펙트를 켜고, 내 캐릭터면 조준·점프를 풀고 멈춘 뒤 시점 회전을 포함한 모든 입력을 막고 게이지를 가득 채워 둔다. 끝나면 게이지는 0이다.
	// 기절이 끝나면 보호 시간 동안 머리 위에 방패 이펙트를 켜고, 내 캐릭터면 게이지 아이콘을 방패로 바꿔 남은 초를 표시한다.
	void StartStun(float duration_seconds);
	virtual void Jump() override;

	// 공격한 쪽 위치를 받아 화면 중앙 둘레에 피격 방향을 표시한다. 내가 조종하는 캐릭터에만 위젯이 있다.
	void ShowDamageDirection(const FVector& attacker_location);

	// 다른 플레이어 캐릭터가 총알을 히트박스로 받게 한다. 메시에 히트박스 물리 에셋을 붙여 보이는 채널만 막고, 캡슐은 총알을 통과시킨다.
	void EnableHitboxCollision();

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
	// 처음 쓸 때 불러오는 에셋 목록. 게임 시작 전에 미리 불러 둔다.
	TArray<FSoftObjectPath> GetPreloadAssets() const;
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
	virtual void CalcCamera(float delta_time, FMinimalViewInfo& out_result) override;

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

	UPROPERTY(EditDefaultsOnly, Category = "Player|Status")
	TSoftObjectPtr<UNiagaraSystem> stun_effect_;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Status")
	TSoftObjectPtr<UNiagaraSystem> protection_effect_;

	// 로컬 플레이에서 내 캐릭터에 테두리로 그려 확인할 히트박스
	UPROPERTY(EditDefaultsOnly, Category = "Player|Debug")
	TSoftObjectPtr<UPhysicsAsset> hitbox_asset_;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera", meta = (UseComponentPicker))
	FComponentReference first_person_camera_reference_;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Camera", meta = (UseComponentPicker))
	FComponentReference third_person_camera_reference_;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Scope")
	TSubclassOf<UUserWidget> scope_widget_class_;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Equipment")
	TSubclassOf<UEquipmentWidget> equipment_widget_class_;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Status")
	TSubclassOf<UStunGaugeWidget> stun_gauge_widget_class_;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Status")
	TSubclassOf<UDamageDirectionWidget> damage_direction_widget_class_;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Scope", meta = (ClampMin = "5", ClampMax = "170"))
	float normal_field_of_view_ = 90.0f;

	// 조준 화면이 걸음에 따라 머리 높이를 따라 오르내리는 비율
	UPROPERTY(EditDefaultsOnly, Category = "Player|Scope", meta = (ClampMin = "0"))
	float aim_camera_bob_scale_ = 6.0f;

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

	// 조준 카메라의 원래 위치와, 출렁임을 구할 때 기준이 되는 머리 높이
	FVector first_person_camera_location_ = FVector::ZeroVector;
	double aim_head_height_ = 0.0;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> scope_widget_;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> stun_effect_component_;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> protection_effect_component_;

	UPROPERTY(Transient)
	TObjectPtr<UEquipmentWidget> equipment_widget_;

	UPROPERTY(Transient)
	TObjectPtr<UStunGaugeWidget> stun_gauge_widget_;

	UPROPERTY(Transient)
	TObjectPtr<UDamageDirectionWidget> damage_direction_widget_;

	TMap<EWeaponType, FWeaponAmmoState> weapon_ammo_;
	// 곡괭이 사용 가능 횟수(pickaxe_charges_)가 최대치보다 적을 때 지금 1번이 차기 시작한 월드 시각(초)
	double pickaxe_charge_start_ = 0.0;
	FTimerHandle pickaxe_charge_timer_;
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

	// 내 기절 게이지는 stun_gauge_time_ 시각의 값이고, 표시할 때 초당 감소량만큼 줄인다.
	float stun_gauge_ = 0.0f;
	double stun_gauge_time_ = 0.0;
	bool is_stunned_ = false;
	FTimerHandle stun_timer_;
	FTimerHandle stun_gauge_timer_;
	// 기절이 끝난 뒤 보호가 끝나는 시각
	double protection_end_time_ = 0.0;
	FTimerHandle protection_timer_;

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
	void UpdateStunGaugeDisplay();
	// 지난 시간만큼 곡괭이 사용 가능 횟수를 채우고 장비·인벤토리 표시를 고친다. 다 차면 충전 표시 타이머를 멈춘다.
	void UpdatePickaxeCharges();
	void EndStun();
	// 남은 보호 시간을 게이지에 표시하고, 다 되면 보호를 끝낸다.
	void UpdateProtection();
	void EndProtection();
	// 머리 위 상태 이펙트를 켜고 끈다.
	void SetHeadEffectActive(const TSoftObjectPtr<UNiagaraSystem>& effect, TObjectPtr<UNiagaraComponent>& component, bool is_active);
};
