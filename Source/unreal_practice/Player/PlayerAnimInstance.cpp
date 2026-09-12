#include "PlayerAnimInstance.h"

#include "PlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

void UPlayerAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	character_ = Cast<APlayerCharacter>(TryGetPawnOwner());
}

void UPlayerAnimInstance::NativeUpdateAnimation(float delta_seconds)
{
	Super::NativeUpdateAnimation(delta_seconds);

	APlayerCharacter* character = character_.Get();
	if (!character)
	{
		character = Cast<APlayerCharacter>(TryGetPawnOwner());
		character_ = character;
	}
	if (!character)
	{
		speed_ = 0.0;
		direction_ = 0.0;
		is_in_air_ = false;
		is_holding_rifle_ = false;
		is_aiming_ = false;
		left_hand_ik_location_ = FVector::ZeroVector;
		return;
	}

	const FVector velocity = character->GetVelocity();
	speed_ = velocity.Size2D();
	const FVector local_velocity = character->GetActorRotation().UnrotateVector(velocity.GetSafeNormal2D());
	direction_ = FMath::RadiansToDegrees(FMath::Atan2(local_velocity.Y, local_velocity.X));
	is_in_air_ = character->GetCharacterMovement()->IsFalling();
	const FWeaponSettings* settings = character->GetCurrentWeaponSettings();
	is_holding_rifle_ = settings && settings->uses_rifle_pose;
	is_aiming_ = character->IsAiming();

	// 소켓과 본 조회는 게임 스레드에서만 수행한다.
	USceneComponent* weapon_mesh = character->GetEquippedWeaponMesh();
	const int32 hand_index = character->GetMesh()->GetBoneIndex(TEXT("hand_r"));
	if (settings && !settings->left_hand_grip_socket.IsNone() && IsValid(weapon_mesh) && weapon_mesh->DoesSocketExist(settings->left_hand_grip_socket) && hand_index != INDEX_NONE)
	{
		const FVector grip_position = weapon_mesh->GetSocketLocation(settings->left_hand_grip_socket);
		left_hand_ik_location_ = character->GetMesh()->GetBoneMatrix(hand_index).InverseTransformPosition(grip_position);
	}
}
