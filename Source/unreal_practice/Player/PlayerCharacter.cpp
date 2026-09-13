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
#include "UI/EquipmentWidget.h"

APlayerCharacter::APlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	equipment_widget_class_ = UEquipmentWidget::StaticClass();

	FWeaponSettings pickaxe;
	pickaxe.display_name = NSLOCTEXT("Equipment", "Pickaxe", "PICKAXE");
	pickaxe.selection_key = EKeys::One;
	pickaxe.mesh.ComponentProperty = TEXT("Pickaxe");
	pickaxe.melee_hit.is_enabled = true;
	weapon_settings_.Add(EWeaponType::PICKAXE, pickaxe);

	FWeaponSettings sniper;
	sniper.display_name = NSLOCTEXT("Equipment", "Sniper", "SNIPER");
	sniper.selection_key = EKeys::Two;
	sniper.mesh.ComponentProperty = TEXT("Sniper");
	sniper.hitscan.is_enabled = true;
	sniper.can_aim = true;
	sniper.uses_rifle_pose = true;
	sniper.left_hand_grip_socket = TEXT("LeftHandGrip");
	weapon_settings_.Add(EWeaponType::SNIPER, sniper);

	pickup_action_.should_disable_movement = true;
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
	player_input_component->BindKey(EKeys::SpaceBar, IE_Pressed, this, &APlayerCharacter::Jump);
	player_input_component->BindKey(EKeys::SpaceBar, IE_Released, this, &APlayerCharacter::StopJumping);
}

void APlayerCharacter::Move(const FInputActionValue& value)
{
	if (is_action_locked_)
	{
		return;
	}

	const FVector2D movement = value.Get<FVector2D>();
	GetCharacterMovement()->MaxWalkSpeed = movement.Y < 0.0f ? backward_speed_ : forward_speed_;
	AddMovementInput(GetActorForwardVector(), movement.Y);
	AddMovementInput(GetActorRightVector(), movement.X);
}

void APlayerCharacter::Look(const FInputActionValue& value)
{
	const FVector2D look = value.Get<FVector2D>();
	AddControllerYawInput(look.X);
	AddControllerPitchInput(look.Y);
}

void APlayerCharacter::Jump()
{
	if (!is_action_locked_)
	{
		Super::Jump();
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
		has_pending_melee_hit_ = false;
		const FWeaponSettings* previous_settings = GetCurrentWeaponSettings();
		if (UAnimInstance* anim_instance = GetMesh()->GetAnimInstance(); anim_instance && previous_settings && previous_settings->hitscan.reload_montage)
		{
			anim_instance->Montage_Stop(0.1f, previous_settings->hitscan.reload_montage);
		}
	}
	current_weapon_ = weapon;
	if (const FWeaponAmmoState* ammo = weapon_ammo_.Find(weapon); ammo && ammo->rounds == 0)
	{
		BeginReload(weapon);
	}
	UpdateWeaponVisuals();
	SetAiming(false);
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
		else
		{
			PlayAction(settings->primary_action, &settings->melee_hit);
		}
	}
}

void APlayerCharacter::PickUp()
{
	PlayAction(pickup_action_);
}

void APlayerCharacter::PlayAction(const FActionSettings& action, const FMeleeHitSettings* melee_hit)
{
	UAnimInstance* anim_instance = GetMesh()->GetAnimInstance();
	if (is_action_locked_ || !action.montage || !anim_instance || action.play_rate <= 0.0f)
	{
		return;
	}
	if (anim_instance->Montage_Play(action.montage, action.play_rate) <= 0.0f)
	{
		return;
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
	should_restore_movement_ = action.should_disable_movement;
	if (should_restore_movement_)
	{
		UCharacterMovementComponent* movement = GetCharacterMovement();
		previous_movement_mode_ = movement->MovementMode;
		previous_custom_movement_mode_ = movement->CustomMovementMode;
		movement->StopMovementImmediately();
		movement->DisableMovement();
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
	if (should_restore_movement_)
	{
		GetCharacterMovement()->SetMovementMode(previous_movement_mode_, previous_custom_movement_mode_);
		should_restore_movement_ = false;
	}
}

void APlayerCharacter::ProcessMeleeHit(int32 montage_instance_id)
{
	if (!is_action_locked_ || !has_pending_melee_hit_ || montage_instance_id == INDEX_NONE || montage_instance_id != active_montage_instance_id_)
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
	if (is_action_locked_ || !world || IsActorBeingDestroyed() || !IsValid(weapon_mesh) || !IsValid(camera) || !camera->IsActive() || !weapon_mesh->DoesSocketExist(settings.muzzle_socket))
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

	if (ammo->rounds == 0)
	{
		BeginReload(shot.weapon);
	}
	UpdateEquipmentDisplay();
	on_shot_fired_.Broadcast(shot);
}

void APlayerCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	UpdateEquipmentDisplay();
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
	FEquipmentDisplay display;
	if (const FWeaponSettings* settings = GetCurrentWeaponSettings())
	{
		display.name = settings->display_name;
		display.uses_ammo = settings->hitscan.is_enabled;
		display.should_show_crosshair = settings->hitscan.is_enabled && !is_aiming_;
		if (display.uses_ammo)
		{
			display.capacity = FMath::Max(1, settings->hitscan.magazine_capacity);
			display.total_ammo = FText::AsCultureInvariant(TEXT("∞"));
			if (const FWeaponAmmoState* ammo = weapon_ammo_.Find(current_weapon_))
			{
				display.rounds = ammo->rounds;
				display.is_reloading = GetWorldTimerManager().IsTimerActive(ammo->reload_timer);
			}
		}
	}
	return display;
}

void APlayerCharacter::Reload()
{
	if (is_action_locked_ || !GetWorld() || IsActorBeingDestroyed())
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
	if (!ammo || !settings || !settings->hitscan.is_enabled || ammo->rounds >= FMath::Max(1, settings->hitscan.magazine_capacity) || GetWorldTimerManager().IsTimerActive(ammo->reload_timer))
	{
		return;
	}
	// 장비를 바꿔도 해당 무기의 재장전 시간과 탄약은 유지한다.
	const FTimerDelegate completed = FTimerDelegate::CreateUObject(this, &APlayerCharacter::FinishReload, weapon);
	GetWorldTimerManager().SetTimer(ammo->reload_timer, completed, FMath::Max(0.01f, settings->hitscan.reload_duration), false);
	PlayReloadAnimation(weapon);
}

void APlayerCharacter::PlayReloadAnimation(EWeaponType weapon)
{
	const FWeaponAmmoState* ammo = weapon_ammo_.Find(weapon);
	const FWeaponSettings* settings = weapon_settings_.Find(weapon);
	UAnimInstance* anim_instance = GetMesh()->GetAnimInstance();
	if (weapon != current_weapon_ || is_action_locked_ || !ammo || !settings || !settings->hitscan.reload_montage || !anim_instance || !GetWorldTimerManager().IsTimerActive(ammo->reload_timer))
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
			anim_instance->Montage_Stop(0.1f, settings->hitscan.reload_montage);
		}
	}
	GetWorldTimerManager().ClearTimer(ammo->reload_timer);
	ammo->rounds = FMath::Max(1, settings->hitscan.magazine_capacity);
	UpdateEquipmentDisplay();
}

void APlayerCharacter::UpdateEquipmentDisplay()
{
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
