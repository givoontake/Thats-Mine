#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "weapon_settings.h"
#include "weapon_type.h"
#include "shot_result.h"
#include "weapon_ammo_state.h"
#include "equipment_display.h"
#include "inventory_hint.h"
#include "inventory_item.h"
#include "player_action.h"
#include "item_kind.h"
#include "PlayerCharacter.generated.h"

class UCameraComponent;
class UDamageDirectionWidget;
class UEquipmentWidget;
class UInputAction;
class UNiagaraComponent;
class UNiagaraSystem;
class UPhysicsAsset;
class UMagnifierCountWidget;
class UStunGaugeWidget;
class UHintPopupWidget;
class UInteractPromptWidget;
class UMinimapWidget;
class UCoordinateWidget;
class UPingMarkerWidget;
class UDecalComponent;
class USoundBase;
class AHintBox;
class UUserWidget;
struct FInputActionValue;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMeleeHit, const FHitResult&, hit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnShotFired, const FShotResult&, shot);

DECLARE_MULTICAST_DELEGATE(FOnEquipmentChanged);
DECLARE_MULTICAST_DELEGATE(FOnHintsChanged);
DECLARE_MULTICAST_DELEGATE(FOnItemsChanged);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnActionStarted, EPlayerAction, EWeaponType);

UCLASS()
class THATSMINE_API APlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	// 무기를 바꾼 뒤 쓸 수 있을 때까지 시간(초). 장착 동작은 이 시간에 맞춰 재생하고, 장착 동작이 없는 무기도 이 시간 동안 쓸 수 없다.
	static constexpr float EQUIP_DURATION = 0.5f;

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
	// 다른 플레이어 캐릭터의 발사 궤적을 지금 든 총의 총구에서 end까지 그린다.
	void ShowShotTracer(const FVector& end);
	// 서버가 인정하지 않은 발사의 발사 번호를 되돌린다.
	void CancelShot(EWeaponType weapon);
	// 들고 있는 총으로 지금 쏘면 쓰는 퍼짐 크기(도). 총이 아니면 0이다.
	float GetCurrentShotSpread() const;
	// 서버가 아는 곡괭이 사용 가능 횟수와 다음 1번이 찰 때까지 남은 시간으로 맞춘다.
	void SetPickaxeCharges(int32 charges, int32 remaining_ms);
	// 기절을 시작한다. 별 이펙트를 켜고, 내 캐릭터면 조준·점프를 풀고 멈춘 뒤 시점 회전을 포함한 모든 입력을 막고 게이지를 가득 채워 둔다. 끝나면 게이지는 0이다.
	// 기절이 끝나면 보호 시간 동안 머리 위에 방패 이펙트를 켜고, 내 캐릭터면 게이지 아이콘을 방패로 바꿔 남은 초를 표시한다. 진행 중인 탐색은 취소한다.
	void StartStun(float duration_seconds, float protection_seconds);
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

	// 얻은 힌트를 인벤토리에 넣고 힌트 획득 안내를 띄운다. 이미 가진 힌트는 인벤토리에 다시 넣지 않고 안내만 띄운다.
	UFUNCTION(BlueprintCallable, Category = "Player|Inventory")
	void AddHint(const FInventoryHint& hint);
	// 문장이 같은 힌트를 인벤토리에서 뺀다.
	void RemoveHint(const FText& text);

	void MarkHintsRead();
	// 가진 힌트 목록의 index번째 힌트의 즐겨찾기를 켜거나 끈다.
	void ToggleHintFavorite(int32 index);
	FOnEquipmentChanged on_equipment_changed_;
	FOnHintsChanged on_hints_changed_;

	// 상자에서 얻은 아이템을 가지고 아이템 획득 안내를 띄운다. 지금보다 높은 등급일 때만 바꾼다.
	void AddItem(EItemKind kind, EHintGrade grade);
	// 힌트·아이템·탄약·블록 획득 안내를 띄운다. 내 캐릭터에만 안내 위젯을 만든다. 등급은 아이템만 넘긴다.
	void ShowAcquirePopup(const FText& text, const FText& title, TOptional<EHintGrade> grade);
	// 장비 표시(화면 아래 장비·에임, 인벤토리)를 지금 상태로 다시 그린다.
	void UpdateEquipmentDisplay();
	// 가진 아이템을 인벤토리에 보일 순서(종류 순)로 돌려준다.
	TArray<FInventoryItem> GetInventoryItems() const;
	FOnItemsChanged on_items_changed_;

	UFUNCTION(BlueprintCallable, Category = "Player|Weapon")
	void SelectWeapon(EWeaponType weapon);

	UFUNCTION(BlueprintCallable, Category = "Player|Actions")
	virtual void PrimaryAction();

	UFUNCTION(BlueprintCallable, Category = "Player|Actions")
	void PickUp();

	UFUNCTION(BlueprintCallable, Category = "Player|Actions")
	void Interact();

	// 키 설정이 바뀌었을 때 키 입력을 다시 묶는다.
	void RebindKeys();

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
	// 발이 땅에 닿을 때 발밑 바닥 종류에 맞는 발소리를 재생한다. 이동 애니메이션의 발소리 알림이 부른다.
	void PlayFootstep();

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
	TObjectPtr<UHintPopupWidget> hint_popup_widget_;

	UPROPERTY(Transient)
	TObjectPtr<UDamageDirectionWidget> damage_direction_widget_;

	UPROPERTY(Transient)
	TObjectPtr<UMagnifierCountWidget> magnifier_count_widget_;

	UPROPERTY(Transient)
	TObjectPtr<UInteractPromptWidget> interact_prompt_widget_;

	UPROPERTY(Transient)
	TObjectPtr<UMinimapWidget> minimap_widget_;

	UPROPERTY(Transient)
	TObjectPtr<UCoordinateWidget> coordinate_widget_;

	UPROPERTY(Transient)
	TObjectPtr<UPingMarkerWidget> ping_marker_widget_;

	// 곡괭이를 든 내 캐릭터 발밑에 보물 발견 범위를 그리는 데칼
	UPROPERTY(Transient)
	TObjectPtr<UDecalComponent> pickaxe_range_decal_;

	// 바닥 종류(풀·돌·나무) 순서로 종류마다 같은 수의 변형을 둔 발소리
	UPROPERTY()
	TArray<TObjectPtr<USoundBase>> footstep_sounds_;
	// 같은 발소리가 연달아 나지 않게 기억하는 직전 변형 번호
	int32 last_footstep_variation_ = INDEX_NONE;
	FTimerHandle interact_prompt_timer_;

	TMap<EWeaponType, FWeaponAmmoState> weapon_ammo_;
	// 곡괭이 사용 가능 횟수(pickaxe_charges_)가 최대치보다 적을 때 지금 1번이 차기 시작한 월드 시각(초)
	double pickaxe_charge_start_ = 0.0;
	FTimerHandle pickaxe_charge_timer_;
	TMap<EWeaponType, double> next_fire_times_;

	UPROPERTY(Transient)
	TArray<FInventoryHint> acquired_hints_;

	int32 pickaxe_charges_ = 3;

	bool is_equipping_ = false;
	FTimerHandle equip_timer_;
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
	// 기절이 끝난 뒤 보호 시간과, 보호가 끝나는 시각
	float protection_seconds_ = 0.0f;
	double protection_end_time_ = 0.0;
	FTimerHandle protection_timer_;

	// 아이템 종류마다 가진 가장 높은 등급. 없으면 비어 있다.
	TOptional<EHintGrade> item_grades_[static_cast<int32>(EItemKind::COUNT)];

	// 키 설정의 키로 동작 입력을 묶는다. 이동은 입력 매핑으로 받아 여기서 묶지 않는다.
	void BindGameKeys(UInputComponent* input);
	void Move(const FInputActionValue& value);
	void Look(const FInputActionValue& value);
	void SelectWeaponFromKey(FKey key);
	void UpdateWeaponVisuals();
	void SetAiming(bool is_aiming);
	bool PlayAction(const FActionSettings& action, const FMeleeHitSettings* melee_hit = nullptr);
	void FinishAction(uint32 action_id);
	void FireHitscan(const FHitscanSettings& settings);
	void PlayMuzzleEffect(const FHitscanSettings& settings);
	void PlayFireSound(const FHitscanSettings& settings) const;
	void PlayReloadSound(EWeaponType weapon) const;
	void BeginReload(EWeaponType weapon);
	void FinishReload(EWeaponType weapon);
	void PlayReloadAnimation(EWeaponType weapon);
	void UpdateStunGaugeDisplay();
	// 지난 시간만큼 곡괭이 사용 가능 횟수를 채우고 장비·인벤토리 표시를 고친다. 다 차면 충전 표시 타이머를 멈춘다.
	void UpdatePickaxeCharges();
	void EndStun();
	// 남은 보호 시간을 게이지에 표시하고, 다 되면 보호를 끝낸다.
	void UpdateProtection();
	void EndProtection();
	// 머리 위 상태 이펙트를 켜고 끈다.
	void SetHeadEffectActive(const TSoftObjectPtr<UNiagaraSystem>& effect, TObjectPtr<UNiagaraComponent>& component, bool is_active);
	// 화면 중앙이 가리키는, 아직 열리지 않은 상자 중 캐릭터 중심에서 상자 중심까지 서버의 F키 열기 거리 안인 것. 없으면 nullptr이다.
	const AHintBox* FindInteractBox() const;
	// 열 수 있는 상자나 주울 수 있는 두루마리가 있으면 상호작용 안내를 보인다.
	void UpdateInteractPrompt();
	// 화면 가운데가 가리키는 곳에 핑을 찍는다. 이전 핑은 사라진다.
	void PlacePing();
	// 가진 아이템을 적용한 값. 아이템이 없으면 기본값이다.
	double GetPickaxeChargeSeconds() const;
	float GetStunGaugeDecayPerSecond() const;
	float GetStunGaugeDecayDelay() const;
};
