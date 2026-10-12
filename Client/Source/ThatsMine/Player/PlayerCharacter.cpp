#include "PlayerCharacter.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/GameViewportSubsystem.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "Flow/ClientStateSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Player/ControlSettingsSubsystem.h"
#include "Interaction/BoxLayoutSubsystem.h"
#include "Interaction/HintDropSubsystem.h"
#include "Interaction/HintBox.h"
#include "Interaction/ItemSubsystem.h"
#include "RemoteMovementComponent.h"
#include "UI/EquipmentWidget.h"
#include "UI/Effects/DamageDirectionWidget.h"
#include "UI/Effects/SearchGaugeWidget.h"
#include "UI/Effects/StunGaugeWidget.h"
#include "UI/Hint/HintPopupWidget.h"
#include "settings.h"

#if !UE_BUILD_SHIPPING && __has_include("LocalDebug/Combat/CombatDebug.h")
#include "LocalDebug/Combat/CombatDebug.h"
#define WITH_LOCAL_COMBAT_DEBUG 1
#else
#define WITH_LOCAL_COMBAT_DEBUG 0
#endif

#if !UE_BUILD_SHIPPING && __has_include("LocalDebug/Hitbox/HitboxDebugComponent.h")
#include "LocalDebug/Hitbox/HitboxDebugComponent.h"
#define WITH_LOCAL_HITBOX_DEBUG 1
#else
#define WITH_LOCAL_HITBOX_DEBUG 0
#endif

// 조준 카메라 출렁임의 기준 머리 높이가 실제 머리 높이를 따라가는 속도. 걸음마다의 출렁임보다 충분히 느려야 출렁임이 남는다.
static constexpr float AIM_HEAD_HEIGHT_INTERP_SPEED = 1.0f;

// 기절 게이지를 화면 아래 끝에서 띄우는 거리
static constexpr float STUN_GAUGE_BOTTOM_MARGIN = 48.0f;
// 줄어드는 기절 게이지 표시를 고치는 간격(초)
static constexpr float STUN_GAUGE_DISPLAY_INTERVAL = 0.05f;
// 보호 남은 시간 표시를 고치는 간격(초)
static constexpr float PROTECTION_DISPLAY_INTERVAL = 0.1f;
// 곡괭이 충전 남은 시간 표시를 고치는 간격(초)
static constexpr float PICKAXE_CHARGE_DISPLAY_INTERVAL = 0.25f;
// 탐색 게이지를 화면 가운데에서 아래로 띄우는 거리
static constexpr float SEARCH_GAUGE_CENTER_OFFSET = 90.0f;

// 몽타주를 도중에 멈출 때도 다른 전환과 같은 관성 보간으로 잇는다.
static FMontageBlendSettings MakeInertialBlendOut(float blend_time)
{
	FMontageBlendSettings settings(blend_time);
	settings.BlendMode = EMontageBlendMode::Inertialization;
	return settings;
}

APlayerCharacter::APlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
	GetCharacterMovement()->AirControl = 0.5f;
	GetCharacterMovement()->AirControlBoostMultiplier = 1.0f;

	equipment_widget_class_ = UEquipmentWidget::StaticClass();
	stun_gauge_widget_class_ = UStunGaugeWidget::StaticClass();
	damage_direction_widget_class_ = UDamageDirectionWidget::StaticClass();

	pickaxe_charges_ = PICKAXE_MAX_CHARGES;
	FWeaponSettings pickaxe;
	pickaxe.display_name = NSLOCTEXT("Equipment", "Pickaxe", "PICKAXE");
	pickaxe.selection_key = EKeys::Three;
	pickaxe.mesh.ComponentProperty = TEXT("Pickaxe");
	pickaxe.melee_hit.is_enabled = true;
	pickaxe.uses_pickaxe_pose = true;
	weapon_settings_.Add(EWeaponType::PICKAXE, pickaxe);

	FWeaponSettings sniper;
	sniper.display_name = NSLOCTEXT("Equipment", "Sniper", "Stun Sniper");
	sniper.selection_key = EKeys::One;
	sniper.mesh.ComponentProperty = TEXT("Sniper");
	sniper.hitscan.is_enabled = true;
	sniper.can_aim = true;
	sniper.uses_rifle_pose = true;
	sniper.left_hand_grip_socket = TEXT("LeftHandGrip");
	weapon_settings_.Add(EWeaponType::SNIPER, sniper);

	FWeaponSettings pistol;
	pistol.display_name = NSLOCTEXT("Equipment", "Pistol", "Stun Pistol");
	pistol.selection_key = EKeys::Two;
	pistol.mesh.ComponentProperty = TEXT("Pistol");
	pistol.hitscan.is_enabled = true;
	pistol.hitscan.magazine_capacity = 7;
	pistol.hitscan.fire_interval = 0.2f;
	pistol.primary_action.play_rate = (14.0f / 30.0f) / 0.18f;
	pistol.uses_pistol_pose = true;
	weapon_settings_.Add(EWeaponType::PISTOL, pistol);

	// 돋보기는 곡괭이 자리에 들고, 클릭하면 탐색 동작을 MAGNIFIER_SEARCH_MS 동안 재생한다.
	FWeaponSettings magnifier;
	magnifier.display_name = NSLOCTEXT("Equipment", "Magnifier", "MAGNIFIER");
	magnifier.selection_key = EKeys::Four;
	magnifier.mesh.ComponentProperty = TEXT("Magnifier");
	magnifier.primary_action.play_rate = 1.0f;
	magnifier.uses_pickaxe_pose = true;
	weapon_settings_.Add(EWeaponType::MAGNIFIER, magnifier);

	first_person_camera_reference_.ComponentProperty = TEXT("FirstPersonCamera");
	third_person_camera_reference_.ComponentProperty = TEXT("ThirdPersonCamera");
}

void APlayerCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// 블루프린트 컴포넌트를 한 번 연결해 기존 소켓과 배치 설정을 보존한다.
	first_person_camera_ = Cast<UCameraComponent>(first_person_camera_reference_.GetComponent(this));
	third_person_camera_ = Cast<UCameraComponent>(third_person_camera_reference_.GetComponent(this));
	if (first_person_camera_)
	{
		first_person_camera_location_ = first_person_camera_->GetRelativeLocation();
	}
	weapon_meshes_.Reset();
	weapon_ammo_.Reset();
	for (const auto& [weapon, settings] : weapon_settings_)
	{
		if (settings.hitscan.is_enabled)
		{
			// 탄과 소지 탄 없이 시작한다.
			weapon_ammo_.Add(weapon, FWeaponAmmoState());
		}
		if (USceneComponent* mesh = Cast<USceneComponent>(settings.mesh.GetComponent(this)))
		{
			weapon_meshes_.Add(weapon, mesh);
		}
	}
}

void APlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	UpdateWeaponVisuals();
	SetAiming(false);
}

void APlayerCharacter::EndPlay(const EEndPlayReason::Type end_play_reason)
{
	FinishAction(action_id_);
	++action_id_;
	for (auto& [weapon, ammo] : weapon_ammo_)
	{
		GetWorldTimerManager().ClearTimer(ammo.reload_timer);
		GetWorldTimerManager().ClearTimer(ammo.shot_wait_timer);
	}
	GetWorldTimerManager().ClearTimer(stun_timer_);
	GetWorldTimerManager().ClearTimer(stun_gauge_timer_);
	GetWorldTimerManager().ClearTimer(protection_timer_);
	GetWorldTimerManager().ClearTimer(pickaxe_charge_timer_);
	GetWorldTimerManager().ClearTimer(search_timer_);
	if (equipment_widget_)
	{
		equipment_widget_->RemoveFromParent();
		equipment_widget_ = nullptr;
	}
	if (search_gauge_widget_)
	{
		search_gauge_widget_->RemoveFromParent();
		search_gauge_widget_ = nullptr;
	}
	if (stun_gauge_widget_)
	{
		stun_gauge_widget_->RemoveFromParent();
		stun_gauge_widget_ = nullptr;
	}
	if (hint_popup_widget_)
	{
		hint_popup_widget_->RemoveFromParent();
		hint_popup_widget_ = nullptr;
	}
	if (damage_direction_widget_)
	{
		damage_direction_widget_->RemoveFromParent();
		damage_direction_widget_ = nullptr;
	}
	if (scope_widget_)
	{
		scope_widget_->RemoveFromParent();
		scope_widget_ = nullptr;
	}
	Super::EndPlay(end_play_reason);
}

void APlayerCharacter::SetupPlayerInputComponent(UInputComponent* player_input_component)
{
	Super::SetupPlayerInputComponent(player_input_component);

	if (UEnhancedInputComponent* enhanced_input = Cast<UEnhancedInputComponent>(player_input_component))
	{
		if (move_action_)
		{
			enhanced_input->BindAction(move_action_, ETriggerEvent::Triggered, this, &APlayerCharacter::Move);
		}
		if (mouse_look_action_)
		{
			enhanced_input->BindAction(mouse_look_action_, ETriggerEvent::Triggered, this, &APlayerCharacter::Look);
		}
	}

	for (const auto& [weapon, settings] : weapon_settings_)
	{
		if (settings.selection_key.IsValid())
		{
			player_input_component->BindKey(settings.selection_key, IE_Pressed, this, &APlayerCharacter::SelectWeaponFromKey);
		}
	}
	player_input_component->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &APlayerCharacter::PrimaryAction);
	player_input_component->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &APlayerCharacter::ToggleAim);
	player_input_component->BindKey(EKeys::R, IE_Pressed, this, &APlayerCharacter::Reload);
	player_input_component->BindKey(EKeys::E, IE_Pressed, this, &APlayerCharacter::PickUp);
	player_input_component->BindKey(EKeys::F, IE_Pressed, this, &APlayerCharacter::Interact);
	player_input_component->BindKey(EKeys::SpaceBar, IE_Pressed, this, &APlayerCharacter::Jump);
	player_input_component->BindKey(EKeys::SpaceBar, IE_Released, this, &APlayerCharacter::StopJumping);
}

void APlayerCharacter::Interact()
{
	if (is_stunned_)
	{
		return;
	}
	UCameraComponent* camera = is_aiming_ ? first_person_camera_ : third_person_camera_;
	if (is_action_locked_ || GetCharacterMovement()->IsFalling() || !GetWorld() || !IsValid(camera) || !camera->IsActive())
	{
		return;
	}
	const FVector start = GetCapsuleComponent()->GetComponentLocation();
	const FVector view_direction = camera->GetForwardVector();
	const FVector view_location = camera->GetComponentLocation();
	FCollisionQueryParams query(SCENE_QUERY_STAT(HintBoxInteraction), false, this);
	for (const auto& [weapon, mesh] : weapon_meshes_)
	{
		if (UPrimitiveComponent* component = Cast<UPrimitiveComponent>(mesh))
		{
			query.AddIgnoredComponent(component);
		}
	}
	const FVector aim_start = view_location + view_direction * FMath::Max(0.0, FVector::DotProduct(start - view_location, view_direction));
	const FVector aim_end = aim_start + view_direction * 10000.0;
	FHitResult aim_hit;
	const bool has_aim_hit = GetWorld()->LineTraceSingleByChannel(aim_hit, aim_start, aim_end, ECC_Visibility, query);
	const FVector direction = ((has_aim_hit ? aim_hit.ImpactPoint : aim_end) - start).GetSafeNormal(UE_SMALL_NUMBER, view_direction);
	FHitResult hit;
	// 카메라는 목표만 정하고, 실제 거리는 캐릭터 중심에서 최대 1m로 제한한다.
	if (GetWorld()->LineTraceSingleByChannel(hit, start, start + direction * 100.0, ECC_Visibility, query))
	{
		if (const AHintBox* box = Cast<AHintBox>(hit.GetActor()))
		{
			GetWorld()->GetSubsystem<UBoxLayoutSubsystem>()->RequestOpen(box, false, 0);
			return;
		}
	}
	// 조준한 상자가 없으면 발밑 근처의 떨어진 두루마리를 줍는다.
	GetWorld()->GetSubsystem<UHintDropSubsystem>()->RequestPickup(start);
}

void APlayerCharacter::Move(const FInputActionValue& value)
{
	if (is_stunned_ || is_searching_)
	{
		return;
	}
	ApplyMovementInput(value.Get<FVector2D>());
}

void APlayerCharacter::ApplyMovementInput(const FVector2D& movement)
{
	const FWeaponSettings* settings = GetCurrentWeaponSettings();
	if (is_aiming_ && settings)
	{
		// 조준 중에는 뒤로 이동할 때도 같은 속도로 천천히 움직인다.
		GetCharacterMovement()->MaxWalkSpeed = settings->aiming_move_speed;
	}
	else
	{
		GetCharacterMovement()->MaxWalkSpeed = movement.Y < 0.0f ? forward_speed_ * 0.5f : forward_speed_;
	}
	AddMovementInput(GetActorForwardVector(), movement.Y);
	AddMovementInput(GetActorRightVector(), movement.X);
}

void APlayerCharacter::CalcCamera(float delta_time, FMinimalViewInfo& out_result)
{
	// 조준 화면은 걸음마다 머리가 오르내린 만큼 따라 움직인다.
	// 느리게 따라가는 기준 높이와의 차이만 써서, 자세마다 다른 머리 높이는 빼고 출렁임만 남긴다.
	if (first_person_camera_)
	{
		const double head_height = GetActorTransform().InverseTransformPosition(GetMesh()->GetSocketLocation(TEXT("head"))).Z;
		aim_head_height_ = is_aiming_ ? FMath::FInterpTo(aim_head_height_, head_height, delta_time, AIM_HEAD_HEIGHT_INTERP_SPEED) : head_height;
		first_person_camera_->SetRelativeLocation(first_person_camera_location_ + FVector(0.0, 0.0, (head_height - aim_head_height_) * aim_camera_bob_scale_));
	}
	Super::CalcCamera(delta_time, out_result);
}

void APlayerCharacter::Look(const FInputActionValue& value)
{
	if (is_stunned_)
	{
		return;
	}
	// 조준 중이면 줌 감도, 아니면 일반 감도를 곱한다.
	const UControlSettingsSubsystem* settings = GetGameInstance() ? GetGameInstance()->GetSubsystem<UControlSettingsSubsystem>() : nullptr;
	const float sensitivity = settings ? (is_aiming_ ? settings->GetZoomSensitivity() : settings->GetMouseSensitivity()) : 1.0f;
	const FVector2D look = value.Get<FVector2D>() * sensitivity;
	AddControllerYawInput(look.X);
	AddControllerPitchInput(look.Y);
}

void APlayerCharacter::OnMovementModeChanged(EMovementMode previous_movement_mode, uint8 previous_custom_mode)
{
	Super::OnMovementModeChanged(previous_movement_mode, previous_custom_mode);
	if (!GetCharacterMovement()->IsFalling() || !is_action_locked_)
	{
		return;
	}

	UAnimInstance* anim_instance = GetMesh()->GetAnimInstance();
	FAnimMontageInstance* montage_instance = anim_instance ? anim_instance->GetMontageInstanceForID(active_montage_instance_id_) : nullptr;
	UAnimMontage* montage = montage_instance ? montage_instance->Montage.Get() : nullptr;
	FinishAction(action_id_);
	if (montage)
	{
		anim_instance->Montage_StopWithBlendSettings(MakeInertialBlendOut(0.1f), montage);
	}
}

void APlayerCharacter::SelectWeaponFromKey(FKey key)
{
	if (is_stunned_ || is_searching_)
	{
		return;
	}
	for (const auto& [weapon, settings] : weapon_settings_)
	{
		if (settings.selection_key != key)
		{
			continue;
		}
		// 돋보기는 가진 것이 있어야 든다. 들고 있을 때 다시 누르면 다른 등급도 있으면 쓸 등급을 바꾼다.
		if (weapon == EWeaponType::MAGNIFIER)
		{
			const EHintGrade other_grade = magnifier_grade_ == EHintGrade::S ? EHintGrade::A : EHintGrade::S;
			if (current_weapon_ == EWeaponType::MAGNIFIER)
			{
				if (GetMagnifierCount(other_grade) > 0)
				{
					magnifier_grade_ = other_grade;
					UpdateEquipmentDisplay();
				}
				return;
			}
			if (GetMagnifierCount(magnifier_grade_) + GetMagnifierCount(other_grade) == 0)
			{
				return;
			}
		}
		SelectWeapon(weapon);
		return;
	}
}

void APlayerCharacter::SelectWeapon(EWeaponType weapon)
{
	// 조준을 먼저 해제해야 무기를 바꿀 수 있다.
	if (is_aiming_ || !weapon_settings_.Contains(weapon))
	{
		return;
	}

	const bool has_weapon_changed = current_weapon_ != weapon;
	if (has_weapon_changed)
	{
		++equip_id_;
		is_equipping_ = false;
		has_pending_melee_hit_ = false;
		const FWeaponSettings* previous_settings = GetCurrentWeaponSettings();
		if (UAnimInstance* anim_instance = GetMesh()->GetAnimInstance(); anim_instance && previous_settings)
		{
			if (previous_settings->equip_montage)
			{
				anim_instance->Montage_StopWithBlendSettings(MakeInertialBlendOut(0.1f), previous_settings->equip_montage);
			}
			if (previous_settings->hitscan.reload_montage)
			{
				anim_instance->Montage_StopWithBlendSettings(MakeInertialBlendOut(0.1f), previous_settings->hitscan.reload_montage);
			}
			if (previous_settings->hitscan.is_enabled && previous_settings->primary_action.montage)
			{
				anim_instance->Montage_StopWithBlendSettings(MakeInertialBlendOut(0.1f), previous_settings->primary_action.montage);
			}
		}
	}
	current_weapon_ = weapon;
	UpdateWeaponVisuals();
	SetAiming(false);
	const FWeaponSettings* settings = GetCurrentWeaponSettings();
	UAnimInstance* anim_instance = GetMesh()->GetAnimInstance();
	if (has_weapon_changed && anim_instance)
	{
		anim_instance->RequestSlotGroupInertialization(TEXT("EquipGroup"), 0.2f);
	}
	if (has_weapon_changed && settings->equip_montage && anim_instance)
	{
		if (anim_instance->Montage_Play(settings->equip_montage, 1.0f, EMontagePlayReturnType::MontageLength, 0.0f, false) > 0.0f)
		{
			is_equipping_ = true;
			const uint32 equip_id = equip_id_;
			FOnMontageEnded ended;
			ended.BindWeakLambda(this, [this, equip_id](UAnimMontage*, bool is_interrupted)
			{
				// 빠른 무기 전환 시 이전 장착 동작의 종료가 새 장착 상태를 해제하지 않게 한다.
				if (equip_id != equip_id_)
				{
					return;
				}
				is_equipping_ = false;
				if (!is_interrupted)
				{
					if (const FWeaponAmmoState* ammo = weapon_ammo_.Find(current_weapon_); ammo && ammo->rounds == 0)
					{
						BeginReload(current_weapon_);
					}
					PlayReloadAnimation(current_weapon_);
					UpdateEquipmentDisplay();
				}
			});
			anim_instance->Montage_SetEndDelegate(ended, settings->equip_montage);
		}
	}
	if (const FWeaponAmmoState* ammo = weapon_ammo_.Find(weapon); ammo && ammo->rounds == 0)
	{
		BeginReload(weapon);
	}
	if (has_weapon_changed)
	{
		PlayReloadAnimation(weapon);
	}
}

void APlayerCharacter::UpdateWeaponVisuals()
{
	for (const auto& [weapon, mesh] : weapon_meshes_)
	{
		if (IsValid(mesh))
		{
			mesh->SetVisibility(weapon == current_weapon_);
		}
	}
	UpdateEquipmentDisplay();
}

const FWeaponSettings* APlayerCharacter::GetCurrentWeaponSettings() const
{
	return weapon_settings_.Find(current_weapon_);
}

USceneComponent* APlayerCharacter::GetEquippedWeaponMesh() const
{
	const TObjectPtr<USceneComponent>* mesh = weapon_meshes_.Find(current_weapon_);
	return mesh ? mesh->Get() : nullptr;
}

TArray<FSoftObjectPath> APlayerCharacter::GetPreloadAssets() const
{
	TArray<FSoftObjectPath> assets;
	for (const auto& [weapon, settings] : weapon_settings_)
	{
		if (!settings.hitscan.muzzle_effect.IsNull())
		{
			assets.AddUnique(settings.hitscan.muzzle_effect.ToSoftObjectPath());
		}
	}
	if (!stun_effect_.IsNull())
	{
		assets.AddUnique(stun_effect_.ToSoftObjectPath());
	}
	if (!protection_effect_.IsNull())
	{
		assets.AddUnique(protection_effect_.ToSoftObjectPath());
	}
	if (!hitbox_asset_.IsNull())
	{
		assets.AddUnique(hitbox_asset_.ToSoftObjectPath());
	}
	return assets;
}

void APlayerCharacter::ShowDamageDirection(const FVector& attacker_location)
{
	if (damage_direction_widget_)
	{
		damage_direction_widget_->ShowHit(attacker_location - GetActorLocation(), GetControlRotation().Yaw);
	}
}

void APlayerCharacter::EnableHitboxCollision()
{
	UPhysicsAsset* hitbox_asset = hitbox_asset_.LoadSynchronous();
	if (!hitbox_asset)
	{
		return;
	}
	USkeletalMeshComponent* mesh = GetMesh();
	mesh->SetPhysicsAsset(hitbox_asset, true);
	mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
}

bool APlayerCharacter::IsAiming() const
{
	return is_aiming_;
}

void APlayerCharacter::PrimaryAction()
{
	if (is_stunned_)
	{
		return;
	}
	if (const FWeaponSettings* settings = GetCurrentWeaponSettings())
	{
		if (settings->hitscan.is_enabled)
		{
			FireHitscan(settings->hitscan);
		}
		else if (current_weapon_ == EWeaponType::MAGNIFIER)
		{
			SearchWithMagnifier();
		}
		else
		{
			// 곡괭이는 사용 가능 횟수가 있어야 휘두르고, 최대치에서 쓰면 충전을 시작한다.
			const bool is_pickaxe = current_weapon_ == EWeaponType::PICKAXE;
			UpdatePickaxeCharges();
			if ((is_pickaxe && pickaxe_charges_ <= 0) || !PlayAction(settings->primary_action, &settings->melee_hit))
			{
				return;
			}
			if (is_pickaxe)
			{
				if (pickaxe_charges_ == PICKAXE_MAX_CHARGES)
				{
					pickaxe_charge_start_ = GetWorld()->GetTimeSeconds();
					GetWorldTimerManager().SetTimer(pickaxe_charge_timer_, this, &APlayerCharacter::UpdatePickaxeCharges, PICKAXE_CHARGE_DISPLAY_INTERVAL, true);
				}
				--pickaxe_charges_;
				UpdateEquipmentDisplay();
			}
			on_action_started_.Broadcast(EPlayerAction::MELEE, current_weapon_);
		}
	}
}

void APlayerCharacter::SearchWithMagnifier()
{
	const FWeaponSettings* settings = GetCurrentWeaponSettings();
	if (is_searching_ || GetMagnifierCount(magnifier_grade_) <= 0 || !settings || !PlayAction(settings->primary_action))
	{
		return;
	}
	GetCharacterMovement()->StopMovementImmediately();
	StartSearch();
	GetWorld()->GetSubsystem<UItemSubsystem>()->RequestSearch(magnifier_grade_);
}

void APlayerCharacter::StartSearch()
{
	is_searching_ = true;
	GetWorldTimerManager().SetTimer(search_timer_, this, &APlayerCharacter::EndSearch, MAGNIFIER_SEARCH_MS / 1000.0f, false);
	APlayerController* controller = Cast<APlayerController>(GetController());
	if (!controller || !controller->IsLocalController())
	{
		return;
	}
	if (!search_gauge_widget_)
	{
		search_gauge_widget_ = CreateWidget<USearchGaugeWidget>(controller, USearchGaugeWidget::StaticClass());
		if (search_gauge_widget_)
		{
			// 화면 가운데 조금 아래에 둔다. 뷰포트의 크기·위치 설정 함수는 기준점을 왼쪽 위로 되돌리므로 한 번에 정해 붙인다.
			FGameViewportWidgetSlot slot;
			slot.Anchors = FAnchors(0.5f, 0.5f);
			slot.Alignment = FVector2D(0.5, 0.5);
			slot.Offsets = FMargin(0.0f, SEARCH_GAUGE_CENTER_OFFSET, UStunGaugeWidget::WIDGET_WIDTH, UStunGaugeWidget::WIDGET_HEIGHT);
			slot.ZOrder = 10;
			UGameViewportSubsystem::Get()->AddWidgetForPlayer(search_gauge_widget_, controller->GetLocalPlayer(), slot);
		}
	}
	if (search_gauge_widget_)
	{
		search_gauge_widget_->Start(MAGNIFIER_SEARCH_MS / 1000.0f);
	}
}

void APlayerCharacter::EndSearch()
{
	is_searching_ = false;
	GetWorldTimerManager().ClearTimer(search_timer_);
	if (search_gauge_widget_)
	{
		search_gauge_widget_->Stop();
	}
	// 마지막 돋보기를 썼으면 곡괭이를 든다.
	if (IsLocallyControlled() && current_weapon_ == EWeaponType::MAGNIFIER && GetMagnifierCount(EHintGrade::S) + GetMagnifierCount(EHintGrade::A) == 0)
	{
		SelectWeapon(EWeaponType::PICKAXE);
	}
}

void APlayerCharacter::PickUp()
{
	if (is_stunned_)
	{
		return;
	}
	if (PlayAction(pickup_action_))
	{
		on_action_started_.Broadcast(EPlayerAction::PICKUP, current_weapon_);
	}
}

bool APlayerCharacter::PlayAction(const FActionSettings& action, const FMeleeHitSettings* melee_hit)
{
	UAnimInstance* anim_instance = GetMesh()->GetAnimInstance();
	if (is_action_locked_ || GetCharacterMovement()->IsFalling() || !action.montage || !anim_instance || action.play_rate <= 0.0f)
	{
		return false;
	}
	if (anim_instance->Montage_Play(action.montage, action.play_rate) <= 0.0f)
	{
		return false;
	}

	is_action_locked_ = true;
	const uint32 action_id = ++action_id_;
	const FAnimMontageInstance* montage_instance = anim_instance->GetActiveInstanceForMontage(action.montage);
	active_montage_instance_id_ = montage_instance ? montage_instance->GetInstanceID() : INDEX_NONE;
	has_pending_melee_hit_ = melee_hit && melee_hit->is_enabled;
	if (has_pending_melee_hit_)
	{
		active_melee_settings_ = *melee_hit;
	}

	FOnMontageEnded ended;
	ended.BindWeakLambda(this, [this, action_id](UAnimMontage*, bool)
	{
		FinishAction(action_id);
	});
	anim_instance->Montage_SetEndDelegate(ended, action.montage);

	FOnMontageBlendingOutStarted blending_out;
	blending_out.BindWeakLambda(this, [this, action_id](UAnimMontage*, bool is_interrupted)
	{
		if (is_interrupted)
		{
			FinishAction(action_id);
		}
	});
	anim_instance->Montage_SetBlendingOutDelegate(blending_out, action.montage);
	return true;
}

void APlayerCharacter::FinishAction(uint32 action_id)
{
	// 이전 몽타주의 지연된 종료 콜백이 다음 행동을 해제하지 않게 한다.
	if (action_id != action_id_ || !is_action_locked_)
	{
		return;
	}
	is_action_locked_ = false;
	has_pending_melee_hit_ = false;
	active_montage_instance_id_ = INDEX_NONE;
}

void APlayerCharacter::ProcessMeleeHit(int32 montage_instance_id)
{
	if (!is_action_locked_ || GetCharacterMovement()->IsFalling() || !has_pending_melee_hit_ || montage_instance_id == INDEX_NONE || montage_instance_id != active_montage_instance_id_)
	{
		return;
	}

	// 빗나가더라도 이번 동작의 판정은 소비한다.
	has_pending_melee_hit_ = false;
	UWorld* world = GetWorld();
	UCameraComponent* camera = is_aiming_ ? first_person_camera_ : third_person_camera_;
	if (!world || !IsValid(camera) || !camera->IsActive())
	{
		return;
	}

	const FVector start = GetCapsuleComponent()->GetComponentLocation();
	const float distance = FMath::Clamp(active_melee_settings_.distance, 0.0f, 150.0f);
	FCollisionQueryParams query(SCENE_QUERY_STAT(MeleeHit), false, this);
	for (const auto& [weapon, mesh] : weapon_meshes_)
	{
		if (UPrimitiveComponent* component = Cast<UPrimitiveComponent>(mesh))
		{
			query.AddIgnoredComponent(component);
		}
	}

	const FVector view_direction = camera->GetForwardVector();
	const FVector view_location = camera->GetComponentLocation();
	// 카메라 뒤쪽 거리는 사거리에 포함하지 않고, 캐릭터 뒤의 물체를 목표로 잡지 않는다.
	const FVector aim_start = view_location + view_direction * FMath::Max(0.0, FVector::DotProduct(start - view_location, view_direction));
	const FVector aim_end = aim_start + view_direction * 10000.0;
	FHitResult aim_hit;
	const bool has_aim_hit = world->LineTraceSingleByChannel(aim_hit, aim_start, aim_end, active_melee_settings_.trace_channel, query);
	const FVector aim_point = has_aim_hit ? aim_hit.ImpactPoint : aim_end;
	const FVector direction = (aim_point - start).GetSafeNormal(UE_SMALL_NUMBER, view_direction);
	const FVector end = start + direction * distance;
	FHitResult hit;
	const bool has_hit = world->LineTraceSingleByChannel(hit, start, end, active_melee_settings_.trace_channel, query);

#if WITH_LOCAL_COMBAT_DEBUG
	CombatDebug::DrawMelee(world, start, end, has_hit, hit);
#endif

	if (has_hit)
	{
		on_melee_hit_.Broadcast(hit);
	}
}

void APlayerCharacter::FireHitscan(const FHitscanSettings& settings)
{
	UWorld* world = GetWorld();
	USceneComponent* weapon_mesh = GetEquippedWeaponMesh();
	UCameraComponent* camera = is_aiming_ ? first_person_camera_ : third_person_camera_;
	if (is_equipping_ || is_action_locked_ || !world || IsActorBeingDestroyed() || !IsValid(weapon_mesh) || !IsValid(camera) || !camera->IsActive() || !weapon_mesh->DoesSocketExist(settings.muzzle_socket))
	{
		return;
	}
	FWeaponAmmoState* ammo = weapon_ammo_.Find(current_weapon_);
	if (!ammo || ammo->rounds <= 0 || GetWorldTimerManager().IsTimerActive(ammo->reload_timer))
	{
		return;
	}
	const double now = world->GetTimeSeconds();
	if (const double* next_fire_time = next_fire_times_.Find(current_weapon_); next_fire_time && now < *next_fire_time)
	{
		return;
	}

	// 명중 여부와 관계없이 발사 간격을 소비하고, 무기 전환 중에도 유지한다.
	next_fire_times_.Add(current_weapon_, now + FMath::Max(0.01f, settings.fire_interval));
	--ammo->rounds;
	on_action_started_.Broadcast(EPlayerAction::FIRE, current_weapon_);
	PlayMuzzleEffect(settings);
	FShotResult shot;
	shot.weapon = current_weapon_;
	shot.muzzle_location = weapon_mesh->GetSocketLocation(settings.muzzle_socket);
	shot.trace_start = shot.muzzle_location;
	const float distance = FMath::Max(1.0f, settings.distance);
	FCollisionQueryParams query(SCENE_QUERY_STAT(HitscanShot), false, this);
	query.bFindInitialOverlaps = true;
	for (const auto& [weapon, mesh] : weapon_meshes_)
	{
		if (UPrimitiveComponent* component = Cast<UPrimitiveComponent>(mesh))
		{
			query.AddIgnoredComponent(component);
		}
	}

	// 손이 벽을 관통해도 검사 시작점은 충돌 캡슐 안쪽에 남긴다.
	const UCapsuleComponent* capsule = GetCapsuleComponent();
	const float max_height = FMath::Max(0.0f, capsule->GetScaledCapsuleHalfHeight() - capsule->GetScaledCapsuleRadius());
	const FVector body_start = capsule->GetComponentLocation() + capsule->GetUpVector() * FMath::Clamp(settings.body_height, -max_height, max_height);
	shot.is_muzzle_blocked = world->SweepSingleByChannel(shot.hit, body_start, shot.muzzle_location, FQuat::Identity, settings.trace_channel, FCollisionShape::MakeSphere(1.0f), query);
	if (shot.is_muzzle_blocked)
	{
		shot.has_hit = true;
		shot.trace_start = body_start;
		shot.trace_end = shot.hit.ImpactPoint;
	}
	else
	{
		const FVector view_direction = camera->GetForwardVector();
		const FVector view_location = camera->GetComponentLocation();
		// 3인칭 카메라와 캐릭터 사이의 물체 때문에 뒤로 발사하지 않게 한다.
		const FVector aim_start = view_location + view_direction * FMath::Max(0.0, FVector::DotProduct(shot.muzzle_location - view_location, view_direction));
		const FVector aim_end = aim_start + view_direction * distance;
		FHitResult aim_hit;
		const bool has_aim_hit = world->LineTraceSingleByChannel(aim_hit, aim_start, aim_end, settings.trace_channel, query);
		const FVector aim_point = has_aim_hit ? aim_hit.ImpactPoint : aim_end;
		const FVector shot_direction = (aim_point - shot.muzzle_location).GetSafeNormal(UE_SMALL_NUMBER, view_direction);
		shot.trace_end = shot.muzzle_location + shot_direction * distance;
		shot.has_hit = world->LineTraceSingleByChannel(shot.hit, shot.trace_start, shot.trace_end, settings.trace_channel, query);
		if (shot.has_hit)
		{
			shot.trace_end = shot.hit.ImpactPoint;
		}
	}

#if WITH_LOCAL_COMBAT_DEBUG
	CombatDebug::DrawShot(world, body_start, shot);
#endif

	if (ammo->rounds == 0 && shot.weapon != EWeaponType::PISTOL)
	{
		BeginReload(shot.weapon);
	}
	const FWeaponSettings* weapon_settings = GetCurrentWeaponSettings();
	UAnimInstance* anim_instance = GetMesh()->GetAnimInstance();
	float fire_duration = 0.0f;
	if (anim_instance && weapon_settings && weapon_settings->primary_action.montage)
	{
		const FActionSettings& action = weapon_settings->primary_action;
		fire_duration = anim_instance->Montage_Play(action.montage, action.play_rate, EMontagePlayReturnType::Duration, 0.0f, false);
	}
	if (ammo->rounds == 0 && shot.weapon == EWeaponType::PISTOL)
	{
		if (fire_duration > 0.0f)
		{
			// 마지막 권총 반동이 끝난 뒤 재장전을 시작한다. 대기 중에는 재장전 동작을 재생하지 않는다.
			ammo->is_waiting_for_shot = true;
			// 실행 중인 타이머를 지우면 이 람다도 함께 해제되므로, 람다 안에서 대기 타이머를 지우지 않는다.
			const FTimerDelegate completed = FTimerDelegate::CreateWeakLambda(this, [this, weapon = shot.weapon]()
			{
				weapon_ammo_.FindChecked(weapon).is_waiting_for_shot = false;
				BeginReload(weapon);
				UpdateEquipmentDisplay();
			});
			GetWorldTimerManager().SetTimer(ammo->shot_wait_timer, completed, fire_duration, false);
		}
		else
		{
			BeginReload(shot.weapon);
		}
	}
	UpdateEquipmentDisplay();
	on_shot_fired_.Broadcast(shot);
	if (shot.has_hit)
	{
		if (const AHintBox* box = Cast<AHintBox>(shot.hit.GetActor()))
		{
			GetWorld()->GetSubsystem<UBoxLayoutSubsystem>()->RequestOpen(box, true, static_cast<uint8>(shot.weapon));
		}
	}
}

void APlayerCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	UpdateEquipmentDisplay();
	APlayerController* controller = Cast<APlayerController>(GetController());
	if (!stun_gauge_widget_ && stun_gauge_widget_class_ && controller && controller->IsLocalController())
	{
		stun_gauge_widget_ = CreateWidget<UStunGaugeWidget>(controller, stun_gauge_widget_class_);
		if (stun_gauge_widget_)
		{
			// 화면 하단 중앙에 빈 게이지로 둔다. 누적 값은 서버에서 받아 SetStunProgress로 바꾼다.
			// 뷰포트의 크기·위치 설정 함수는 기준점을 왼쪽 위로 되돌리므로, 기준점·정렬·위치·크기를 한 번에 정해 붙인다.
			FGameViewportWidgetSlot slot;
			slot.Anchors = FAnchors(0.5f, 1.0f);
			slot.Alignment = FVector2D(0.5, 1.0);
			slot.Offsets = FMargin(0.0f, -STUN_GAUGE_BOTTOM_MARGIN, UStunGaugeWidget::WIDGET_WIDTH, UStunGaugeWidget::WIDGET_HEIGHT);
			slot.ZOrder = 10;
			UGameViewportSubsystem::Get()->AddWidgetForPlayer(stun_gauge_widget_, controller->GetLocalPlayer(), slot);
		}
	}
	if (!damage_direction_widget_ && damage_direction_widget_class_ && controller && controller->IsLocalController())
	{
		damage_direction_widget_ = CreateWidget<UDamageDirectionWidget>(controller, damage_direction_widget_class_);
		if (damage_direction_widget_)
		{
			// 화면 전체에 깔아 화면 중앙을 기준으로 피격 방향 호를 그린다.
			FGameViewportWidgetSlot slot;
			slot.ZOrder = 9;
			UGameViewportSubsystem::Get()->AddWidgetForPlayer(damage_direction_widget_, controller->GetLocalPlayer(), slot);
		}
	}

#if WITH_LOCAL_HITBOX_DEBUG
	// 로컬 플레이에서는 내 히트박스를 테두리로 그려 확인한다.
	if (controller && controller->IsLocalController() && GetGameInstance()->GetSubsystem<UClientStateSubsystem>()->IsLocalMode() && !FindComponentByClass<UHitboxDebugComponent>())
	{
		UHitboxDebugComponent* hitbox_debug = NewObject<UHitboxDebugComponent>(this);
		hitbox_debug->SetHitbox(GetMesh(), hitbox_asset_.LoadSynchronous());
		hitbox_debug->RegisterComponent();
	}
#endif
}

FRotator APlayerCharacter::GetViewRotation() const
{
	// 조종자가 없는 다른 플레이어는 받은 시선 상하 각도를 쓴다.
	if (!GetController())
	{
		if (const URemoteMovementComponent* remote_movement = FindComponentByClass<URemoteMovementComponent>())
		{
			return FRotator(remote_movement->GetViewPitch(), GetActorRotation().Yaw, 0.0f);
		}
	}
	return Super::GetViewRotation();
}

void APlayerCharacter::PlayMuzzleEffect(const FHitscanSettings& settings)
{
	USceneComponent* weapon_mesh = GetEquippedWeaponMesh();
	UNiagaraSystem* effect = settings.muzzle_effect.LoadSynchronous();
	if (!effect || !IsValid(weapon_mesh))
	{
		return;
	}
	UNiagaraComponent* component = UNiagaraFunctionLibrary::SpawnSystemAttached(effect, weapon_mesh, settings.muzzle_socket, FVector::ZeroVector, FRotator::ZeroRotator, FVector(settings.muzzle_effect_scale), EAttachLocation::SnapToTarget, true, ENCPoolMethod::AutoRelease);
	// 무기 메시는 원본 단위 때문에 배율이 매우 작아서, 이펙트 크기는 무기 배율을 물려받지 않고 설정값을 그대로 쓴다.
	if (component)
	{
		component->SetUsingAbsoluteScale(true);
	}
}

void APlayerCharacter::Jump()
{
	if (!is_stunned_ && !is_searching_)
	{
		Super::Jump();
	}
}

void APlayerCharacter::SetAmmo(EWeaponType weapon, int32 rounds, int32 spare_rounds)
{
	FWeaponAmmoState* ammo = weapon_ammo_.Find(weapon);
	if (!ammo)
	{
		return;
	}
	ammo->rounds = rounds;
	ammo->spare_rounds = spare_rounds;
	// 들고 있는 총의 탄창이 비었을 때 소지 탄이 생기면, 탄이 떨어졌을 때처럼 바로 재장전한다.
	if (weapon == current_weapon_ && rounds == 0 && spare_rounds > 0)
	{
		BeginReload(weapon);
	}
	UpdateEquipmentDisplay();
}

void APlayerCharacter::SetPickaxeCharges(int32 charges, int32 remaining_ms)
{
	pickaxe_charges_ = charges;
	if (charges >= PICKAXE_MAX_CHARGES)
	{
		GetWorldTimerManager().ClearTimer(pickaxe_charge_timer_);
	}
	else
	{
		pickaxe_charge_start_ = GetWorld()->GetTimeSeconds() - (GetPickaxeChargeSeconds() - remaining_ms / 1000.0);
		GetWorldTimerManager().SetTimer(pickaxe_charge_timer_, this, &APlayerCharacter::UpdatePickaxeCharges, PICKAXE_CHARGE_DISPLAY_INTERVAL, true);
	}
	UpdateEquipmentDisplay();
}

void APlayerCharacter::UpdatePickaxeCharges()
{
	if (pickaxe_charges_ < PICKAXE_MAX_CHARGES)
	{
		const double charge_seconds = GetPickaxeChargeSeconds();
		const int32 gained = FMath::FloorToInt32((GetWorld()->GetTimeSeconds() - pickaxe_charge_start_) / charge_seconds);
		if (gained > 0)
		{
			pickaxe_charges_ = FMath::Min(PICKAXE_MAX_CHARGES, pickaxe_charges_ + gained);
			pickaxe_charge_start_ += gained * charge_seconds;
		}
	}
	if (pickaxe_charges_ >= PICKAXE_MAX_CHARGES)
	{
		GetWorldTimerManager().ClearTimer(pickaxe_charge_timer_);
	}
	// 인벤토리는 들고 있지 않은 곡괭이의 남은 시간도 표시하므로 들고 있는 무기와 관계없이 고친다.
	UpdateEquipmentDisplay();
}

void APlayerCharacter::SetStunGauge(float gauge)
{
	stun_gauge_ = gauge;
	stun_gauge_time_ = GetWorld()->GetTimeSeconds();
	UpdateStunGaugeDisplay();
	// 게이지가 남아 있는 동안 줄어드는 값을 표시한다.
	if (stun_gauge_ > 0.0f)
	{
		GetWorldTimerManager().SetTimer(stun_gauge_timer_, this, &APlayerCharacter::UpdateStunGaugeDisplay, STUN_GAUGE_DISPLAY_INTERVAL, true);
	}
}

void APlayerCharacter::UpdateStunGaugeDisplay()
{
	// 마지막으로 오른 뒤 감소 대기 시간이 지나야 줄기 시작한다.
	const double decay_time = FMath::Max(0.0, GetWorld()->GetTimeSeconds() - stun_gauge_time_ - GetStunGaugeDecayDelay());
	const float gauge = FMath::Max(0.0f, stun_gauge_ - GetStunGaugeDecayPerSecond() * static_cast<float>(decay_time));
	if (stun_gauge_widget_)
	{
		stun_gauge_widget_->SetStunProgress(gauge / STUN_GAUGE_MAX);
	}
	if (gauge <= 0.0f)
	{
		GetWorldTimerManager().ClearTimer(stun_gauge_timer_);
	}
}

void APlayerCharacter::StartStun(float duration_seconds, float protection_seconds)
{
	if (is_searching_)
	{
		const FWeaponSettings* magnifier = weapon_settings_.Find(EWeaponType::MAGNIFIER);
		UAnimInstance* anim_instance = GetMesh()->GetAnimInstance();
		if (magnifier && magnifier->primary_action.montage && anim_instance)
		{
			anim_instance->Montage_StopWithBlendSettings(MakeInertialBlendOut(0.1f), magnifier->primary_action.montage);
		}
		EndSearch();
	}
	protection_seconds_ = protection_seconds;
	EndProtection();
	SetHeadEffectActive(stun_effect_, stun_effect_component_, true);
	is_stunned_ = true;
	if (IsLocallyControlled())
	{
		SetAiming(false);
		StopJumping();
		GetCharacterMovement()->StopMovementImmediately();
		GetWorldTimerManager().ClearTimer(stun_gauge_timer_);
		if (stun_gauge_widget_)
		{
			stun_gauge_widget_->SetStunProgress(1.0f);
		}
	}
	GetWorldTimerManager().SetTimer(stun_timer_, this, &APlayerCharacter::EndStun, FMath::Max(duration_seconds, 0.01f), false);
}

void APlayerCharacter::EndStun()
{
	is_stunned_ = false;
	SetHeadEffectActive(stun_effect_, stun_effect_component_, false);
	stun_gauge_ = 0.0f;
	if (stun_gauge_widget_)
	{
		stun_gauge_widget_->SetStunProgress(0.0f);
	}
	SetHeadEffectActive(protection_effect_, protection_effect_component_, true);
	protection_end_time_ = GetWorld()->GetTimeSeconds() + protection_seconds_;
	UpdateProtection();
	GetWorldTimerManager().SetTimer(protection_timer_, this, &APlayerCharacter::UpdateProtection, PROTECTION_DISPLAY_INTERVAL, true);
}

void APlayerCharacter::UpdateProtection()
{
	const double remaining = protection_end_time_ - GetWorld()->GetTimeSeconds();
	if (remaining <= 0.0)
	{
		EndProtection();
		return;
	}
	if (stun_gauge_widget_)
	{
		stun_gauge_widget_->SetProtectionSeconds(FMath::CeilToInt32(remaining));
	}
}

void APlayerCharacter::EndProtection()
{
	GetWorldTimerManager().ClearTimer(protection_timer_);
	SetHeadEffectActive(protection_effect_, protection_effect_component_, false);
	if (stun_gauge_widget_)
	{
		stun_gauge_widget_->SetProtectionSeconds(0);
	}
}

void APlayerCharacter::SetHeadEffectActive(const TSoftObjectPtr<UNiagaraSystem>& effect, TObjectPtr<UNiagaraComponent>& component, bool is_active)
{
	if (!is_active)
	{
		if (component)
		{
			component->DestroyComponent();
			component = nullptr;
		}
		return;
	}
	UNiagaraSystem* system = effect.LoadSynchronous();
	if (component || !system)
	{
		return;
	}
	component = UNiagaraFunctionLibrary::SpawnSystemAttached(system, GetMesh(), TEXT("head"), FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, false);
	// 머리 뼈 위치만 따라가고, 고리는 뼈의 방향과 배율을 따르지 않아 늘 수평으로 같은 크기로 있다.
	if (component)
	{
		component->SetUsingAbsoluteRotation(true);
		component->SetUsingAbsoluteScale(true);
		component->SetWorldRotation(FRotator::ZeroRotator);
		component->SetWorldScale3D(FVector::OneVector);
	}
}

void APlayerCharacter::PlayRemoteAction(EPlayerAction action, EWeaponType weapon)
{
	UAnimInstance* anim_instance = GetMesh()->GetAnimInstance();
	if (!anim_instance)
	{
		return;
	}
	if (action == EPlayerAction::PICKUP)
	{
		if (pickup_action_.montage)
		{
			anim_instance->Montage_Play(pickup_action_.montage, pickup_action_.play_rate);
		}
		return;
	}
	// 재장전은 무기를 바꾼 뒤에도 이어지므로 들고 있는 무기의 재장전만 재생한다. 발사와 휘두르기는 그 무기를 든 상태에서만 일어난다.
	if (weapon != current_weapon_)
	{
		if (action == EPlayerAction::RELOAD)
		{
			return;
		}
		SelectWeapon(weapon);
	}
	const FWeaponSettings* settings = GetCurrentWeaponSettings();
	if (!settings)
	{
		return;
	}
	if (action == EPlayerAction::RELOAD)
	{
		if (UAnimMontage* montage = settings->hitscan.reload_montage)
		{
			anim_instance->Montage_Play(montage, montage->GetPlayLength() / FMath::Max(0.01f, settings->hitscan.reload_duration), EMontagePlayReturnType::MontageLength, 0.0f, false);
		}
		return;
	}
	if (action == EPlayerAction::FIRE)
	{
		PlayMuzzleEffect(settings->hitscan);
	}
	// 발사와 휘두르기는 들고 있는 무기의 기본 동작을 재생한다.
	if (settings->primary_action.montage)
	{
		anim_instance->Montage_Play(settings->primary_action.montage, settings->primary_action.play_rate, EMontagePlayReturnType::MontageLength, 0.0f, action == EPlayerAction::MELEE);
	}
	if (action == EPlayerAction::SEARCH)
	{
		StartSearch();
	}
}

void APlayerCharacter::UnPossessed()
{
	if (equipment_widget_)
	{
		equipment_widget_->RemoveFromParent();
		equipment_widget_ = nullptr;
	}
	if (stun_gauge_widget_)
	{
		stun_gauge_widget_->RemoveFromParent();
		stun_gauge_widget_ = nullptr;
	}
	if (hint_popup_widget_)
	{
		hint_popup_widget_->RemoveFromParent();
		hint_popup_widget_ = nullptr;
	}
	if (damage_direction_widget_)
	{
		damage_direction_widget_->RemoveFromParent();
		damage_direction_widget_ = nullptr;
	}
	if (search_gauge_widget_)
	{
		search_gauge_widget_->RemoveFromParent();
		search_gauge_widget_ = nullptr;
	}
	Super::UnPossessed();
}

FEquipmentDisplay APlayerCharacter::GetEquipmentDisplay() const
{
	return GetWeaponDisplay(current_weapon_);
}

FEquipmentDisplay APlayerCharacter::GetWeaponDisplay(EWeaponType weapon) const
{
	FEquipmentDisplay display;
	if (const FWeaponSettings* settings = weapon_settings_.Find(weapon))
	{
		display.name = settings->display_name;
		display.uses_ammo = settings->hitscan.is_enabled || weapon == EWeaponType::PICKAXE;
		display.should_show_crosshair = settings->hitscan.is_enabled && !is_aiming_;
		if (display.uses_ammo)
		{
			display.capacity = FMath::Max(1, settings->hitscan.magazine_capacity);
			if (const FWeaponAmmoState* ammo = weapon_ammo_.Find(weapon))
			{
				display.rounds = ammo->rounds;
				display.total_ammo = FText::AsNumber(ammo->spare_rounds);
				display.is_reloading = ammo->is_waiting_for_shot || GetWorldTimerManager().IsTimerActive(ammo->reload_timer);
			}
		}
		// 곡괭이는 사용 가능 횟수와, 다 차 있지 않으면 다음 1번이 찰 때까지 남은 시간(분:초, 초 올림)을 표시한다.
		if (weapon == EWeaponType::PICKAXE)
		{
			display.rounds = pickaxe_charges_;
			if (pickaxe_charges_ < PICKAXE_MAX_CHARGES)
			{
				const double charge_seconds = GetPickaxeChargeSeconds();
				const double elapsed = FMath::Max(0.0, GetWorld()->GetTimeSeconds() - pickaxe_charge_start_);
				const int32 remaining = FMath::CeilToInt32(charge_seconds - FMath::Fmod(elapsed, charge_seconds));
				display.total_ammo = FText::AsCultureInvariant(FString::Printf(TEXT("%d:%02d"), remaining / 60, remaining % 60));
			}
		}
		// 돋보기는 쓸 등급과 그 등급의 개수를 표시한다.
		if (weapon == EWeaponType::MAGNIFIER)
		{
			display.name = FText::Format(NSLOCTEXT("Equipment", "MagnifierGrade", "{0} {1}"), settings->display_name, FText::AsCultureInvariant(magnifier_grade_ == EHintGrade::S ? TEXT("S") : TEXT("A")));
			display.uses_ammo = true;
			display.rounds = GetMagnifierCount(magnifier_grade_);
		}
	}
	return display;
}

void APlayerCharacter::AddHint(const FInventoryHint& hint)
{
	if (!acquired_hints_.ContainsByPredicate([&hint](const FInventoryHint& acquired) { return acquired.text.EqualTo(hint.text); }))
	{
		acquired_hints_.Add(hint);
		on_hints_changed_.Broadcast();
	}
	ShowAcquirePopup(hint, NSLOCTEXT("Hint", "NewHint", "새 힌트 획득!"));
}

void APlayerCharacter::ShowAcquirePopup(const FInventoryHint& entry, const FText& title)
{
	APlayerController* controller = Cast<APlayerController>(GetController());
	if (!hint_popup_widget_ && controller && controller->IsLocalController())
	{
		hint_popup_widget_ = CreateWidget<UHintPopupWidget>(controller, UHintPopupWidget::StaticClass());
		if (hint_popup_widget_)
		{
			hint_popup_widget_->AddToViewport(10);
		}
	}
	if (hint_popup_widget_)
	{
		hint_popup_widget_->Enqueue(entry, title);
	}
}

// 아이템 효과 문장. 수치는 쓰지 않는다. 패시브는 늘거나 주는 것만 쓰고 S 등급은 "대폭"을 붙이며, 돋보기는 S가 정밀, A가 넓은 탐색이다.
static FText GetItemText(EItemKind kind, EHintGrade grade)
{
	const FText degree = grade == EHintGrade::S ? NSLOCTEXT("Item", "Large", "대폭 ") : FText::GetEmpty();
	switch (kind)
	{
	case EItemKind::PICKAXE_CHARGE:
		return FText::Format(NSLOCTEXT("Item", "PickaxeCharge", "곡괭이 충전 시간 {0}감소"), degree);
	case EItemKind::STUN_GAUGE_DECAY:
		return FText::Format(NSLOCTEXT("Item", "StunGaugeDecay", "기절 게이지 감소량 {0}증가"), degree);
	case EItemKind::STUN_GAUGE_DECAY_DELAY:
		return FText::Format(NSLOCTEXT("Item", "StunGaugeDecayDelay", "기절 게이지 감소 대기 {0}감소"), degree);
	case EItemKind::STUN_PROTECTION:
		return FText::Format(NSLOCTEXT("Item", "StunProtection", "기절 후 보호 시간 {0}증가"), degree);
	case EItemKind::MAGNIFIER:
		return grade == EHintGrade::S ? NSLOCTEXT("Item", "MagnifierPrecise", "돋보기 · 정밀 탐색") : NSLOCTEXT("Item", "MagnifierWide", "돋보기 · 넓은 탐색");
	default:
		return FText::GetEmpty();
	}
}

void APlayerCharacter::AddItem(EItemKind kind, EHintGrade grade)
{
	if (kind == EItemKind::MAGNIFIER)
	{
		// 쓸 등급의 돋보기가 없으면 얻은 등급을 쓴다.
		if (GetMagnifierCount(magnifier_grade_) == 0)
		{
			magnifier_grade_ = grade;
		}
		++magnifier_counts_[static_cast<int32>(grade)];
	}
	else
	{
		// 등급 값은 높은 등급일수록 작다.
		TOptional<EHintGrade>& current = item_grades_[static_cast<int32>(kind)];
		if (!current || grade < *current)
		{
			current = grade;
		}
	}
	on_items_changed_.Broadcast();
	UpdateEquipmentDisplay();
	FInventoryHint entry;
	entry.grade = grade;
	entry.text = GetItemText(kind, grade);
	ShowAcquirePopup(entry, NSLOCTEXT("Item", "NewItem", "새 아이템 획득!"));
}

void APlayerCharacter::ConsumeMagnifier(EHintGrade grade)
{
	int32& count = magnifier_counts_[static_cast<int32>(grade)];
	count = FMath::Max(0, count - 1);
	const EHintGrade other_grade = magnifier_grade_ == EHintGrade::S ? EHintGrade::A : EHintGrade::S;
	if (GetMagnifierCount(magnifier_grade_) == 0 && GetMagnifierCount(other_grade) > 0)
	{
		magnifier_grade_ = other_grade;
	}
	// 탐색이 끝나기 전이면 탐색이 끝날 때 곡괭이로 바꾼다.
	if (!is_searching_ && current_weapon_ == EWeaponType::MAGNIFIER && GetMagnifierCount(magnifier_grade_) == 0)
	{
		SelectWeapon(EWeaponType::PICKAXE);
	}
	on_items_changed_.Broadcast();
	UpdateEquipmentDisplay();
}

TArray<FInventoryItem> APlayerCharacter::GetInventoryItems() const
{
	TArray<FInventoryItem> items;
	for (const EHintGrade grade : { EHintGrade::S, EHintGrade::A })
	{
		if (GetMagnifierCount(grade) > 0)
		{
			items.Add({ grade, FText::Format(NSLOCTEXT("Item", "MagnifierCount", "{0} × {1}"), GetItemText(EItemKind::MAGNIFIER, grade), FText::AsNumber(GetMagnifierCount(grade))) });
		}
	}
	for (int32 kind = 0; kind < UE_ARRAY_COUNT(item_grades_); ++kind)
	{
		if (item_grades_[kind])
		{
			items.Add({ *item_grades_[kind], GetItemText(static_cast<EItemKind>(kind), *item_grades_[kind]) });
		}
	}
	return items;
}

double APlayerCharacter::GetPickaxeChargeSeconds() const
{
	const TOptional<EHintGrade>& grade = item_grades_[static_cast<int32>(EItemKind::PICKAXE_CHARGE)];
	return (grade ? PICKAXE_CHARGE_MS - ITEM_PICKAXE_CHARGE_REDUCTIONS_MS[static_cast<int32>(*grade)] : PICKAXE_CHARGE_MS) / 1000.0;
}

float APlayerCharacter::GetStunGaugeDecayPerSecond() const
{
	const TOptional<EHintGrade>& grade = item_grades_[static_cast<int32>(EItemKind::STUN_GAUGE_DECAY)];
	return grade ? ITEM_STUN_GAUGE_DECAYS_PER_SECOND[static_cast<int32>(*grade)] : STUN_GAUGE_DECAY_PER_SECOND;
}

float APlayerCharacter::GetStunGaugeDecayDelay() const
{
	const TOptional<EHintGrade>& grade = item_grades_[static_cast<int32>(EItemKind::STUN_GAUGE_DECAY_DELAY)];
	return (grade ? ITEM_STUN_GAUGE_DECAY_DELAYS_MS[static_cast<int32>(*grade)] : STUN_GAUGE_DECAY_DELAY_MS) / 1000.0f;
}

void APlayerCharacter::RemoveHint(const FText& text)
{
	const int32 index = acquired_hints_.IndexOfByPredicate([&text](const FInventoryHint& hint) { return hint.text.EqualTo(text); });
	if (index != INDEX_NONE)
	{
		acquired_hints_.RemoveAt(index);
		on_hints_changed_.Broadcast();
	}
}

void APlayerCharacter::MarkHintsRead()
{
	for (FInventoryHint& hint : acquired_hints_)
	{
		hint.is_new = false;
	}
}

void APlayerCharacter::Reload()
{
	if (is_stunned_)
	{
		return;
	}
	if (is_equipping_ || is_action_locked_ || !GetWorld() || IsActorBeingDestroyed())
	{
		return;
	}
	BeginReload(current_weapon_);
	UpdateEquipmentDisplay();
}

void APlayerCharacter::BeginReload(EWeaponType weapon)
{
	FWeaponAmmoState* ammo = weapon_ammo_.Find(weapon);
	const FWeaponSettings* settings = weapon_settings_.Find(weapon);
	if ((weapon == current_weapon_ && is_equipping_) || !ammo || !settings || !settings->hitscan.is_enabled || ammo->spare_rounds <= 0 || ammo->rounds >= FMath::Max(1, settings->hitscan.magazine_capacity) || ammo->is_waiting_for_shot || GetWorldTimerManager().IsTimerActive(ammo->reload_timer))
	{
		return;
	}
	// 장비를 바꿔도 해당 무기의 재장전 시간과 탄약은 유지한다.
	const FTimerDelegate completed = FTimerDelegate::CreateUObject(this, &APlayerCharacter::FinishReload, weapon);
	GetWorldTimerManager().SetTimer(ammo->reload_timer, completed, FMath::Max(0.01f, settings->hitscan.reload_duration), false);
	PlayReloadAnimation(weapon);
	on_action_started_.Broadcast(EPlayerAction::RELOAD, weapon);
}

void APlayerCharacter::PlayReloadAnimation(EWeaponType weapon)
{
	const FWeaponAmmoState* ammo = weapon_ammo_.Find(weapon);
	const FWeaponSettings* settings = weapon_settings_.Find(weapon);
	UAnimInstance* anim_instance = GetMesh()->GetAnimInstance();
	if (weapon != current_weapon_ || is_equipping_ || is_action_locked_ || !ammo || ammo->is_waiting_for_shot || !settings || !settings->hitscan.reload_montage || !anim_instance || !GetWorldTimerManager().IsTimerActive(ammo->reload_timer))
	{
		return;
	}
	UAnimMontage* montage = settings->hitscan.reload_montage;
	if (anim_instance->Montage_IsPlaying(montage))
	{
		return;
	}
	const float duration = GetWorldTimerManager().GetTimerRate(ammo->reload_timer);
	const float play_rate = montage->GetPlayLength() / duration;
	const float position = GetWorldTimerManager().GetTimerElapsed(ammo->reload_timer) * play_rate;
	anim_instance->Montage_Play(montage, play_rate, EMontagePlayReturnType::MontageLength, position, false);
}

void APlayerCharacter::FinishReload(EWeaponType weapon)
{
	FWeaponAmmoState* ammo = weapon_ammo_.Find(weapon);
	const FWeaponSettings* settings = weapon_settings_.Find(weapon);
	if (!ammo || !settings)
	{
		return;
	}
	if (weapon == current_weapon_ && settings->hitscan.reload_montage)
	{
		if (UAnimInstance* anim_instance = GetMesh()->GetAnimInstance())
		{
			anim_instance->Montage_StopWithBlendSettings(MakeInertialBlendOut(0.1f), settings->hitscan.reload_montage);
		}
	}
	GetWorldTimerManager().ClearTimer(ammo->reload_timer);
	// 소지 탄에서 탄창이 빈 만큼 채운다.
	const int32 loaded = FMath::Min(FMath::Max(1, settings->hitscan.magazine_capacity) - ammo->rounds, ammo->spare_rounds);
	ammo->rounds += loaded;
	ammo->spare_rounds -= loaded;
	UpdateEquipmentDisplay();
}

void APlayerCharacter::UpdateEquipmentDisplay()
{
	on_equipment_changed_.Broadcast();
	APlayerController* controller = Cast<APlayerController>(GetController());
	if (!controller || !controller->IsLocalController() || !controller->GetLocalPlayer() || IsActorBeingDestroyed())
	{
		return;
	}
	if (!equipment_widget_ && equipment_widget_class_)
	{
		equipment_widget_ = CreateWidget<UEquipmentWidget>(controller, equipment_widget_class_);
		if (equipment_widget_)
		{
			equipment_widget_->SetVisibility(ESlateVisibility::HitTestInvisible);
			equipment_widget_->AddToViewport(10);
		}
	}
	if (equipment_widget_)
	{
		equipment_widget_->SetEquipment(GetEquipmentDisplay());
	}
}

void APlayerCharacter::ToggleAim()
{
	if (is_stunned_)
	{
		return;
	}
	if (const FWeaponSettings* settings = GetCurrentWeaponSettings(); settings && settings->can_aim)
	{
		SetAiming(!is_aiming_);
	}
}

void APlayerCharacter::SetAiming(bool is_aiming)
{
	const FWeaponSettings* settings = GetCurrentWeaponSettings();
	if (!first_person_camera_ || !third_person_camera_)
	{
		return;
	}

	is_aiming_ = is_aiming && settings && settings->can_aim;
	if (is_aiming_)
	{
		third_person_camera_->Deactivate();
		first_person_camera_->SetFieldOfView(settings->aiming_field_of_view);
		first_person_camera_->Activate();

		// 로컬 플레이어가 처음 조준할 때 한 번 생성해 재사용한다.
		APlayerController* player_controller = Cast<APlayerController>(GetController());
		if (!scope_widget_ && scope_widget_class_ && player_controller && player_controller->IsLocalController())
		{
			scope_widget_ = CreateWidget<UUserWidget>(player_controller, scope_widget_class_);
			if (scope_widget_)
			{
				scope_widget_->AddToViewport();
			}
		}
	}
	else
	{
		first_person_camera_->Deactivate();
		third_person_camera_->SetFieldOfView(normal_field_of_view_);
		third_person_camera_->Activate();
	}

	if (scope_widget_)
	{
		scope_widget_->SetVisibility(is_aiming_ ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	UpdateEquipmentDisplay();
}
