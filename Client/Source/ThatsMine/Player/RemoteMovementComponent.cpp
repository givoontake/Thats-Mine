#include "RemoteMovementComponent.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PlayerCharacter.h"
#include "movement_settings.h"

// 받은 상태까지 옮기는 시간. 서버가 틱마다 보내는 간격과 같다.
static constexpr float MOVE_SECONDS = MOVE_STEP_MS / 1000.0f;

URemoteMovementComponent::URemoteMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void URemoteMovementComponent::BeginPlay()
{
	Super::BeginPlay();
	APlayerCharacter* character = CastChecked<APlayerCharacter>(GetOwner());
	UCharacterMovementComponent* movement = character->GetCharacterMovement();
	// 조종자 없는 캐릭터는 생성할 때 이 설정이 켜져 있어야 기본 이동 모드를 받으므로, 켜서 이동 모드를 받은 뒤 다시 끈다.
	// 끈 뒤에는 이동 컴포넌트가 이동을 계산하지 않고, 위치는 받은 상태로만 옮긴다.
	movement->bRunPhysicsWithNoController = true;
	movement->SetDefaultMovementMode();
	movement->bRunPhysicsWithNoController = false;
	movement->bOrientRotationToMovement = false;
	movement->bUseControllerDesiredRotation = false;
	// 이동 컴포넌트가 루트 모션 등으로 옮겨도 받은 상태로 옮긴 위치가 남도록 그 뒤에 옮긴다.
	PrimaryComponentTick.AddPrerequisite(movement, movement->PrimaryComponentTick);
	// 다른 플레이어와는 충돌하지 않고 통과한다.
	character->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	// 내 총알은 다른 플레이어의 히트박스에 맞는다.
	character->EnableHitboxCollision();
}

void URemoteMovementComponent::ApplyState(const FRemoteMoveState& state, uint32 tick_number)
{
	StartMove(state, tick_number);
	ApplyMovementMode(state);
}

void URemoteMovementComponent::ApplyMoveStop(const FVector& location, uint32 tick_number)
{
	FRemoteMoveState state = state_;
	state.location = location;
	state.velocity.X = 0.0f;
	state.velocity.Y = 0.0f;
	StartMove(state, tick_number);
	ApplyMovementMode(state);
}

void URemoteMovementComponent::ApplyRollback(const FRemoteMoveState& state, uint32 tick_number)
{
	APlayerCharacter* character = CastChecked<APlayerCharacter>(GetOwner());
	character->SetActorLocationAndRotation(state.location, FRotator(0.0f, state.yaw, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
	view_pitch_ = state.view_pitch;
	start_location_ = state.location;
	start_yaw_ = state.yaw;
	start_view_pitch_ = state.view_pitch;
	move_elapsed_ = MOVE_SECONDS;
	state_ = state;
	tick_number_ = tick_number;
	has_state_ = true;
	ApplyMovementMode(state);
}

uint8 URemoteMovementComponent::GetTickProgress() const
{
	return static_cast<uint8>(FMath::RoundToInt(FMath::Clamp(move_elapsed_ / MOVE_SECONDS, 0.0f, 1.0f) * 100.0f));
}

void URemoteMovementComponent::StartMove(const FRemoteMoveState& state, uint32 tick_number)
{
	APlayerCharacter* character = CastChecked<APlayerCharacter>(GetOwner());
	if (!has_state_)
	{
		character->SetActorLocationAndRotation(state.location, FRotator(0.0f, state.yaw, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
		view_pitch_ = state.view_pitch;
	}
	start_location_ = character->GetActorLocation();
	start_yaw_ = character->GetActorRotation().Yaw;
	start_view_pitch_ = view_pitch_;
	move_elapsed_ = has_state_ ? 0.0f : MOVE_SECONDS;
	state_ = state;
	tick_number_ = tick_number;
	has_state_ = true;
}

void URemoteMovementComponent::ApplyMovementMode(const FRemoteMoveState& state)
{
	UCharacterMovementComponent* movement = CastChecked<APlayerCharacter>(GetOwner())->GetCharacterMovement();
	movement->Velocity = state.velocity;
	if (state.is_falling != movement->IsFalling())
	{
		movement->SetMovementMode(state.is_falling ? MOVE_Falling : MOVE_Walking);
	}
}

void URemoteMovementComponent::SetAiming(bool is_aiming)
{
	APlayerCharacter* character = CastChecked<APlayerCharacter>(GetOwner());
	if (character->IsAiming() != is_aiming)
	{
		character->ToggleAim();
	}
}

// 조준 중에는 무기를 바꿀 수 없으므로 조준을 풀고 바꾼다. 조준은 뒤에 오는 조준 패킷으로 다시 맞춘다.
void URemoteMovementComponent::SetWeapon(EWeaponType weapon)
{
	APlayerCharacter* character = CastChecked<APlayerCharacter>(GetOwner());
	if (character->GetCurrentWeapon() == weapon)
	{
		return;
	}
	if (character->IsAiming())
	{
		character->ToggleAim();
	}
	character->SelectWeapon(weapon);
}

// 옮기기 시작한 위치·방향에서 받은 상태까지 흐른 시간 비율만큼 옮긴다. 다 옮긴 뒤 다음 상태가 늦으면 그 자리에 둔다.
void URemoteMovementComponent::TickComponent(float delta_seconds, ELevelTick tick_type, FActorComponentTickFunction* this_tick_function)
{
	Super::TickComponent(delta_seconds, tick_type, this_tick_function);
	if (!has_state_)
	{
		return;
	}
	APlayerCharacter* character = CastChecked<APlayerCharacter>(GetOwner());
	move_elapsed_ = FMath::Min(move_elapsed_ + delta_seconds, MOVE_SECONDS);
	const float alpha = move_elapsed_ / MOVE_SECONDS;
	const float yaw = start_yaw_ + FMath::FindDeltaAngleDegrees(start_yaw_, state_.yaw) * alpha;
	character->SetActorLocationAndRotation(FMath::Lerp(start_location_, state_.location, alpha), FRotator(0.0f, yaw, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
	view_pitch_ = FMath::Lerp(start_view_pitch_, state_.view_pitch, alpha);
}
