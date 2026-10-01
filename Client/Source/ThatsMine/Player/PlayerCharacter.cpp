#include "PlayerCharacter.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Blueprint/UserWidget.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Interaction/HintBox.h"
#include "RemoteMovementComponent.h"
#include "UI/EquipmentWidget.h"

#if !UE_BUILD_SHIPPING && __has_include("LocalDebug/Combat/CombatDebug.h")
#include "LocalDebug/Combat/CombatDebug.h"
#define WITH_LOCAL_COMBAT_DEBUG 1
#else
#define WITH_LOCAL_COMBAT_DEBUG 0
#endif

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

	FWeaponSettings pickaxe;
	pickaxe.display_name = NSLOCTEXT("Equipment", "Pickaxe", "PICKAXE");
	pickaxe.selection_key = EKeys::Three;
	pickaxe.mesh.ComponentProperty = TEXT("Pickaxe");
	pickaxe.melee_hit.is_enabled = true;
	weapon_settings_.Add(EWeaponType::PICKAXE, pickaxe);

	FWeaponSettings sniper;
	sniper.display_name = NSLOCTEXT("Equipment", "Sniper", "SNIPER");
	sniper.selection_key = EKeys::One;
	sniper.mesh.ComponentProperty = TEXT("Sniper");
	sniper.hitscan.is_enabled = true;
	sniper.can_aim = true;
	sniper.uses_rifle_pose = true;
	sniper.left_hand_grip_socket = TEXT("LeftHandGrip");
	weapon_settings_.Add(EWeaponType::SNIPER, sniper);

	FWeaponSettings pistol;
	pistol.display_name = NSLOCTEXT("Equipment", "Pistol", "PISTOL");
	pistol.selection_key = EKeys::Two;
	pistol.mesh.ComponentProperty = TEXT("Pistol");
	pistol.hitscan.is_enabled = true;
	pistol.hitscan.magazine_capacity = 7;
	pistol.hitscan.fire_interval = 0.2f;
	pistol.primary_action.play_rate = (14.0f / 30.0f) / 0.18f;
	pistol.uses_pistol_pose = true;
	weapon_settings_.Add(EWeaponType::PISTOL, pistol);

	first_person_camera_reference_.ComponentProperty = TEXT("FirstPersonCamera");
	third_person_camera_reference_.ComponentProperty = TEXT("ThirdPersonCamera");
}

void APlayerCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// 블루프린트 컴포넌트를 한 번 연결해 기존 소켓과 배치 설정을 보존한다.
	first_person_camera_ = Cast<UCameraComponent>(first_person_camera_reference_.GetComponent(this));
	third_person_camera_ = Cast<UCameraComponent>(third_person_camera_reference_.GetComponent(this));
	weapon_meshes_.Reset();
	weapon_ammo_.Reset();
	for (const auto& [weapon, settings] : weapon_settings_)
	{
		if (settings.hitscan.is_enabled)
		{
			FWeaponAmmoState ammo;
			ammo.rounds = FMath::Max(1, settings.hitscan.magazine_capacity);
			weapon_ammo_.Add(weapon, ammo);
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
	if (equipment_widget_)
	{
		equipment_widget_->RemoveFromParent();
		equipment_widget_ = nullptr;
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
		if (AHintBox* box = Cast<AHintBox>(hit.GetActor()))
		{
			box->Activate(false);
		}
	}
}

void APlayerCharacter::Move(const FInputActionValue& value)
{
	ApplyMovementInput(value.Get<FVector2D>());
}

void APlayerCharacter::ApplyMovementInput(const FVector2D& movement)
{
	GetCharacterMovement()->MaxWalkSpeed = movement.Y < 0.0f ? forward_speed_ * 0.5f : forward_speed_;
	AddMovementInput(GetActorForwardVector(), movement.Y);
	AddMovementInput(GetActorRightVector(), movement.X);
}

void APlayerCharacter::Look(const FInputActionValue& value)
{
	const FVector2D look = value.Get<FVector2D>();
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
	for (const auto& [weapon, settings] : weapon_settings_)
	{
		if (settings.selection_key == key)
		{
			SelectWeapon(weapon);
			return;
		}
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
	return assets;
}

bool APlayerCharacter::IsAiming() const
{
	return is_aiming_;
}

void APlayerCharacter::PrimaryAction()
{
	if (const FWeaponSettings* settings = GetCurrentWeaponSettings())
	{
		if (settings->hitscan.is_enabled)
		{
			FireHitscan(settings->hitscan);
		}
		else if (PlayAction(settings->primary_action, &settings->melee_hit))
		{
			on_action_started_.Broadcast(EPlayerAction::MELEE, current_weapon_);
		}
	}
}

void APlayerCharacter::PickUp()
{
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
		if (AHintBox* box = Cast<AHintBox>(shot.hit.GetActor()))
		{
			box->Activate(true);
		}
	}
}

void APlayerCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	UpdateEquipmentDisplay();
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
}

void APlayerCharacter::UnPossessed()
{
	if (equipment_widget_)
	{
		equipment_widget_->RemoveFromParent();
		equipment_widget_ = nullptr;
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
		display.uses_ammo = settings->hitscan.is_enabled;
		display.should_show_crosshair = settings->hitscan.is_enabled && !is_aiming_;
		if (display.uses_ammo)
		{
			display.capacity = FMath::Max(1, settings->hitscan.magazine_capacity);
			display.total_ammo = FText::AsCultureInvariant(TEXT("∞"));
			if (const FWeaponAmmoState* ammo = weapon_ammo_.Find(weapon))
			{
				display.rounds = ammo->rounds;
				display.is_reloading = ammo->is_waiting_for_shot || GetWorldTimerManager().IsTimerActive(ammo->reload_timer);
			}
		}
	}
	return display;
}

void APlayerCharacter::AddHint(const FInventoryHint& hint)
{
	acquired_hints_.Add(hint);
	on_hints_changed_.Broadcast();
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
	if ((weapon == current_weapon_ && is_equipping_) || !ammo || !settings || !settings->hitscan.is_enabled || ammo->rounds >= FMath::Max(1, settings->hitscan.magazine_capacity) || ammo->is_waiting_for_shot || GetWorldTimerManager().IsTimerActive(ammo->reload_timer))
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
	ammo->rounds = FMath::Max(1, settings->hitscan.magazine_capacity);
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
