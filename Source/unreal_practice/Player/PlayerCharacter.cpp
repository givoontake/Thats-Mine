#include "PlayerCharacter.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Blueprint/UserWidget.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"

APlayerCharacter::APlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	FWeaponSettings pickaxe;
	pickaxe.selection_key = EKeys::One;
	pickaxe.mesh.ComponentProperty = TEXT("Pickaxe");
	weapon_settings_.Add(EWeaponType::PICKAXE, pickaxe);

	FWeaponSettings sniper;
	sniper.selection_key = EKeys::Two;
	sniper.mesh.ComponentProperty = TEXT("Sniper");
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
	for (const auto& [weapon, settings] : weapon_settings_)
	{
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

	current_weapon_ = weapon;
	UpdateWeaponVisuals();
	SetAiming(false);
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
		PlayAction(settings->primary_action);
	}
}

void APlayerCharacter::PickUp()
{
	PlayAction(pickup_action_);
}

void APlayerCharacter::PlayAction(const FActionSettings& action)
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
	if (should_restore_movement_)
	{
		GetCharacterMovement()->SetMovementMode(previous_movement_mode_, previous_custom_movement_mode_);
		should_restore_movement_ = false;
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
}
