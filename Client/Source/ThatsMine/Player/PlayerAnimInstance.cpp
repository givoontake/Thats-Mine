#include "PlayerAnimInstance.h"

#include "PlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

// 기준 방향을 바꾸지 않고 유지하는 여유 각도. 경계 근처에서 기준 방향이 떨리지 않게 한다.
static constexpr float CARDINAL_HYSTERESIS_DEGREES = 10.0f;
// 이보다 느리면 기준 방향을 바꾸지 않는다.
static constexpr double MIN_LOCOMOTION_SPEED = 10.0;
// 하체 돌리기와 상체 흔들림을 켜고 끌 때 걸리는 시간. 한 번에 바뀌어 자세가 튀지 않게 한다.
static constexpr float POSE_ALPHA_BLEND_SECONDS = 0.2f;

static float GetCardinalAngle(ELocomotionCardinal cardinal)
{
	switch (cardinal)
	{
	case ELocomotionCardinal::BACKWARD: return 180.0f;
	case ELocomotionCardinal::LEFT: return -90.0f;
	case ELocomotionCardinal::RIGHT: return 90.0f;
	default: return 0.0f;
	}
}

// 앞뒤는 대각선까지 포함하고(±67.5도), 옆은 정면 옆 근처(±22.5도)만 맡는다.
static bool IsInCardinal(ELocomotionCardinal cardinal, float direction, float margin)
{
	const float half_range = cardinal == ELocomotionCardinal::FORWARD || cardinal == ELocomotionCardinal::BACKWARD ? 67.5f : 22.5f;
	return FMath::Abs(FMath::FindDeltaAngleDegrees(GetCardinalAngle(cardinal), direction)) <= half_range + margin;
}

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
		locomotion_cardinal_ = ELocomotionCardinal::FORWARD;
		orientation_warp_angle_ = 0.0f;
		orientation_warp_alpha_ = 1.0f;
		is_in_air_ = false;
		is_holding_rifle_ = false;
		is_holding_pistol_ = false;
		is_holding_pickaxe_ = false;
		is_aiming_ = false;
		upper_body_sway_alpha_ = 0.0f;
		left_hand_ik_location_ = FVector::ZeroVector;
		aim_pitch_ = 0.0f;
		spine_aim_rotation_ = FRotator::ZeroRotator;
		chest_aim_rotation_ = FRotator::ZeroRotator;
		head_look_alpha_ = 0.0f;
		return;
	}

	const FVector velocity = character->GetVelocity();
	speed_ = velocity.Size2D();
	const FVector local_velocity = character->GetActorRotation().UnrotateVector(velocity.GetSafeNormal2D());
	direction_ = FMath::RadiansToDegrees(FMath::Atan2(local_velocity.Y, local_velocity.X));
	if (speed_ < MIN_LOCOMOTION_SPEED)
	{
		orientation_warp_angle_ = 0.0f;
	}
	else
	{
		if (!IsInCardinal(locomotion_cardinal_, direction_, CARDINAL_HYSTERESIS_DEGREES))
		{
			for (const ELocomotionCardinal cardinal : { ELocomotionCardinal::FORWARD, ELocomotionCardinal::BACKWARD, ELocomotionCardinal::LEFT, ELocomotionCardinal::RIGHT })
			{
				if (IsInCardinal(cardinal, direction_, 0.0f))
				{
					locomotion_cardinal_ = cardinal;
					break;
				}
			}
		}
		orientation_warp_angle_ = FMath::FindDeltaAngleDegrees(GetCardinalAngle(locomotion_cardinal_), direction_);
	}
	is_in_air_ = character->GetCharacterMovement()->IsFalling();
	// 공중에서는 발이 땅에 닿지 않으므로 하체를 돌리지 않는다.
	orientation_warp_alpha_ = FMath::FInterpConstantTo(orientation_warp_alpha_, is_in_air_ ? 0.0f : 1.0f, delta_seconds, 1.0f / POSE_ALPHA_BLEND_SECONDS);
	const FWeaponSettings* settings = character->GetCurrentWeaponSettings();
	is_holding_rifle_ = settings && settings->uses_rifle_pose;
	is_holding_pistol_ = settings && settings->uses_pistol_pose;
	is_holding_pickaxe_ = settings && settings->uses_pickaxe_pose;
	is_aiming_ = character->IsAiming();
	// 무기 상체 자세에만 몸통 움직임을 조금 더하고, 조준 중에는 자세가 흔들리지 않게 뺀다.
	const bool has_upper_body_pose = is_holding_rifle_ || is_holding_pistol_ || is_holding_pickaxe_;
	const float sway_target = has_upper_body_pose && !is_aiming_ ? upper_body_sway_ratio_ : 0.0f;
	upper_body_sway_alpha_ = FMath::FInterpConstantTo(upper_body_sway_alpha_, sway_target, delta_seconds, FMath::Max(upper_body_sway_ratio_, 0.01f) / POSE_ALPHA_BLEND_SECONDS);
	const float view_pitch = FMath::FindDeltaAngleDegrees(character->GetActorRotation().Pitch, character->GetViewRotation().Pitch);
	const float head_pitch = FMath::Clamp(view_pitch, -FMath::Clamp(head_pitch_down_limit_, 0.0f, 89.0f), FMath::Clamp(head_pitch_up_limit_, 0.0f, 89.0f));
	// 고개는 모든 자세의 최종 출력에서 같은 목표를 즉시 바라본다. 상체 회전은 중복해서 더하지 않는다.
	const FVector head_direction = character->GetActorQuat().RotateVector(FRotator(head_pitch, 0.0f, 0.0f).Vector());
	head_look_location_ = character->GetMesh()->GetSocketLocation(TEXT("head")) + head_direction * 100000.0;
	// 돋보기로 탐색하는 동안은 고개를 탐색 동작에 맡긴다.
	head_look_alpha_ = FMath::FInterpConstantTo(head_look_alpha_, character->IsSearching() ? 0.0f : 1.0f, delta_seconds, 1.0f / POSE_ALPHA_BLEND_SECONDS);
	if (is_holding_rifle_ || is_holding_pistol_)
	{
		const float target_pitch = FMath::Clamp(view_pitch, -FMath::Clamp(settings->aim_pitch_down_limit, 0.0f, 89.0f), FMath::Clamp(settings->aim_pitch_up_limit, 0.0f, 89.0f));
		aim_pitch_ = FMath::FInterpTo(aim_pitch_, target_pitch, delta_seconds, FMath::Max(0.1f, settings->aim_pitch_interp_speed));
		// 캐릭터의 상하 회전을 메시 좌표로 변환해 본의 로컬 축에 영향을 받지 않게 한다.
		const FQuat actor_to_mesh = character->GetMesh()->GetComponentQuat().Inverse() * character->GetActorQuat();
		spine_aim_rotation_ = (actor_to_mesh * FRotator(aim_pitch_ * 0.35f, 0.0f, 0.0f).Quaternion() * actor_to_mesh.Inverse()).Rotator();
		chest_aim_rotation_ = (actor_to_mesh * FRotator(aim_pitch_ * 0.65f, 0.0f, 0.0f).Quaternion() * actor_to_mesh.Inverse()).Rotator();
	}
	else
	{
		aim_pitch_ = 0.0f;
		spine_aim_rotation_ = FRotator::ZeroRotator;
		chest_aim_rotation_ = FRotator::ZeroRotator;
	}

	// 소켓과 본 조회는 게임 스레드에서만 수행한다.
	USceneComponent* weapon_mesh = character->GetEquippedWeaponMesh();
	const int32 hand_index = character->GetMesh()->GetBoneIndex(TEXT("hand_r"));
	if (settings && !settings->left_hand_grip_socket.IsNone() && IsValid(weapon_mesh) && weapon_mesh->DoesSocketExist(settings->left_hand_grip_socket) && hand_index != INDEX_NONE)
	{
		const FVector grip_position = weapon_mesh->GetSocketLocation(settings->left_hand_grip_socket);
		left_hand_ik_location_ = character->GetMesh()->GetBoneMatrix(hand_index).InverseTransformPosition(grip_position);
	}
}
