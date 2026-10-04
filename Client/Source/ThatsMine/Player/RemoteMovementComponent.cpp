#include "RemoteMovementComponent.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PlayerCharacter.h"

URemoteMovementComponent::URemoteMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void URemoteMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	APlayerCharacter* character = CastChecked<APlayerCharacter>(GetOwner());
	UCharacterMovementComponent* movement = character->GetCharacterMovement();
	// 조종자가 없어도 받은 입력으로 이동을 계산하고, 몸 방향은 받은 값으로 정한다.
	movement->bRunPhysicsWithNoController = true;
	// 조종자 없는 캐릭터는 생성할 때 이 설정이 켜져 있어야 기본 이동 모드를 받으므로, 여기서 켠 뒤 직접 지정한다.
	movement->SetDefaultMovementMode();
	movement->bOrientRotationToMovement = false;
	movement->bUseControllerDesiredRotation = false;
	// 입력, 방향, 보간 이동을 먼저 넣은 뒤 이동을 계산한다.
	movement->PrimaryComponentTick.AddPrerequisite(this, PrimaryComponentTick);
	// 다른 플레이어와는 충돌하지 않고 통과한다.
	character->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	// 내 총알은 다른 플레이어의 히트박스에 맞는다.
	character->EnableHitboxCollision();
}

void URemoteMovementComponent::ApplyState(const FRemoteMoveState& state)
{
	APlayerCharacter* character = CastChecked<APlayerCharacter>(GetOwner());
	UCharacterMovementComponent* movement = character->GetCharacterMovement();

	// 처음 받았거나 차이가 크면 바로 옮기고, 아니면 차이를 보간 이동으로 메운다. 이전 보간은 새 좌표 기준으로 다시 시작한다.
	const FVector difference = state.location - character->GetActorLocation();
	if (!has_state_ || difference.Size() > SNAP_DISTANCE)
	{
		character->SetActorLocation(state.location, false, nullptr, ETeleportType::TeleportPhysics);
		movement->Velocity = state.velocity;
		correction_ = FVector::ZeroVector;
		correction_remaining_ = 0.0f;
	}
	else
	{
		correction_ = difference;
		correction_remaining_ = CORRECTION_SECONDS;
	}
	if (!has_state_)
	{
		character->SetActorRotation(FRotator(0.0f, state.yaw, 0.0f));
		view_pitch_ = state.view_pitch;
	}

	// 점프는 받은 속도로 시작하고, 착지는 걷기로 돌린다.
	if (state.is_falling != movement->IsFalling())
	{
		if (state.is_falling)
		{
			movement->Velocity = state.velocity;
		}
		movement->SetMovementMode(state.is_falling ? MOVE_Falling : MOVE_Walking);
	}

	// 조준 중에는 무기를 바꿀 수 없으므로 조준 해제, 무기 교체, 조준 순서로 맞춘다.
	if (character->IsAiming() && (!state.is_aiming || character->GetCurrentWeapon() != state.weapon))
	{
		character->ToggleAim();
	}
	if (character->GetCurrentWeapon() != state.weapon)
	{
		character->SelectWeapon(state.weapon);
	}
	if (character->IsAiming() != state.is_aiming)
	{
		character->ToggleAim();
	}

	state_ = state;
	has_state_ = true;
	state_time_ = GetWorld()->GetTimeSeconds();
}

void URemoteMovementComponent::TickComponent(float delta_seconds, ELevelTick tick_type, FActorComponentTickFunction* this_tick_function)
{
	Super::TickComponent(delta_seconds, tick_type, this_tick_function);
	if (!has_state_)
	{
		return;
	}
	APlayerCharacter* character = CastChecked<APlayerCharacter>(GetOwner());

	// 몸 방향과 시선 상하는 최신 값을 짧은 쪽으로 따라간다.
	const float rotation_alpha = 1.0f - FMath::Exp(-delta_seconds / ROTATION_SMOOTH_SECONDS);
	const float yaw = character->GetActorRotation().Yaw;
	character->SetActorRotation(FRotator(0.0f, yaw + FMath::FindDeltaAngleDegrees(yaw, state_.yaw) * rotation_alpha, 0.0f));
	view_pitch_ += (state_.view_pitch - view_pitch_) * rotation_alpha;

	// 받은 좌표와의 차이를 남은 시간에 나눠 캐릭터 위치에 더한다.
	if (correction_remaining_ > 0.0f)
	{
		const float ratio = FMath::Min(1.0f, delta_seconds / correction_remaining_);
		const FVector step = correction_ * ratio;
		character->SetActorLocation(character->GetActorLocation() + step, false, nullptr, ETeleportType::TeleportPhysics);
		correction_ -= step;
		correction_remaining_ -= delta_seconds;
	}

	// 받은 입력으로 이동 계산을 이어 간다. 새 상태가 오래 끊기면 멈춘다.
	if (GetWorld()->GetTimeSeconds() - state_time_ <= STALE_SECONDS)
	{
		character->ApplyMovementInput(state_.move_input);
	}
}
