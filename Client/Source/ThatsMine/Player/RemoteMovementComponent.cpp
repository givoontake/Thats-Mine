#include "RemoteMovementComponent.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PlayerCharacter.h"

// 이동 설정의 마찰과 제동 감속으로, speed에서 입력 없이 설 때까지 가는 거리를 엔진과 같은 방식으로 계산한다.
// 엔진은 프레임마다 감속한 뒤의 속도로 그 프레임을 움직이고, 정지 기준 속도 아래면 멈춘다. 프레임 시간은 이번 프레임 값을 쓴다.
static float PredictBrakingDistance(const UCharacterMovementComponent* movement, float speed, float frame_seconds)
{
	constexpr float MIN_STEP_SECONDS = 0.005f;
	constexpr float MAX_STEP_SECONDS = 0.05f;
	constexpr float STOP_SPEED = 10.0f;
	const float friction = FMath::Max(0.0f, (movement->bUseSeparateBrakingFriction ? movement->BrakingFriction : movement->GroundFriction) * FMath::Max(0.0f, movement->BrakingFrictionFactor));
	const float deceleration = FMath::Max(0.0f, movement->GetMaxBrakingDeceleration());
	if (friction <= 0.0f && deceleration <= 0.0f)
	{
		return 0.0f;
	}
	const float step = FMath::Clamp(frame_seconds, MIN_STEP_SECONDS, MAX_STEP_SECONDS);
	float distance = 0.0f;
	float velocity = speed;
	while (velocity > 0.0f)
	{
		velocity -= (friction * velocity + deceleration) * step;
		if (velocity <= STOP_SPEED)
		{
			velocity = 0.0f;
		}
		distance += velocity * step;
	}
	return distance;
}

URemoteMovementComponent::URemoteMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

// 걷다가 멈춘 입력이면, 보낸 사람이 키를 뗀 위치에서 같은 제동으로 멈출 자리를 정하고 원격 캐릭터를 그 자리에 세운다.
// 원격 캐릭터 위치에 따라 세 가지로 나눈다.
// - 제동 시작 위치보다 뒤: 지금 속도로 그 위치까지 간 뒤 보낸 사람 속도로 바꿔 엔진 제동으로 선다.
// - 제동 시작 위치와 멈출 자리 사이: 멈출 자리에 서도록 일정하게 감속한다.
// - 멈출 자리를 이미 지남: 더 미끄러지지 않게 바로 세우고, 지난 만큼만 받은 위치로 보정한다.
void URemoteMovementComponent::ApplyMoveInput(const FVector2D& move_input, const FVector& location, const FVector2D& sent_velocity)
{
	APlayerCharacter* character = CastChecked<APlayerCharacter>(GetOwner());
	UCharacterMovementComponent* movement = character->GetCharacterMovement();
	if (is_controlled_stop_ && !move_input.IsNearlyZero())
	{
		FinishControlledStop();
	}
	const float speed = FVector2D(movement->Velocity).Size();
	const float sent_speed = sent_velocity.Size();
	const bool is_stop = has_state_ && move_input.IsNearlyZero() && !move_input_.IsNearlyZero();
	if (is_stop && speed > STOP_MIN_SPEED && sent_speed > STOP_ARRIVE_DISTANCE && !movement->IsFalling())
	{
		// 방향과 남은 제동은 보낸 사람 속도로 정한다. 보낸 사람은 키를 뗀 프레임에 이미 한 번 감속한 뒤 보내므로 그 속도를 그대로 쓴다.
		const FVector2D direction = sent_velocity / sent_speed;
		const FVector target = location + FVector(direction * PredictBrakingDistance(movement, sent_speed, GetWorld()->GetDeltaSeconds()), 0.0f);
		const FVector offset = target - character->GetActorLocation();
		const float remaining = FVector2D(offset.X, offset.Y) | direction;
		const float to_start = FVector2D(location.X - character->GetActorLocation().X, location.Y - character->GetActorLocation().Y) | direction;
		if (remaining <= STOP_ARRIVE_DISTANCE)
		{
			movement->Velocity.X = 0.0f;
			movement->Velocity.Y = 0.0f;
			correction_ = offset;
			correction_remaining_ = CORRECTION_SECONDS;
		}
		else
		{
			is_controlled_stop_ = true;
			is_stop_cruise_ = to_start > STOP_ARRIVE_DISTANCE;
			stop_direction_ = direction;
			stop_start_location_ = location;
			stop_sent_speed_ = sent_speed;
			stop_distance_ = remaining;
			stop_speed_ = speed;
			stop_duration_ = 2.0f * remaining / speed;
			stop_elapsed_ = 0.0f;
			// 경로 옆으로 벗어난 만큼과 높이는 보통 보정으로 맞춘다.
			correction_ = offset - FVector(direction * remaining, 0.0f);
			correction_remaining_ = CORRECTION_SECONDS;
			saved_braking_deceleration_ = movement->BrakingDecelerationWalking;
			saved_ground_friction_ = movement->GroundFriction;
			movement->BrakingDecelerationWalking = 0.0f;
			movement->GroundFriction = 0.0f;
		}
	}
	SetMoveInput(move_input);
}

// 멈출 자리까지 남은 거리를 이번 프레임 평균 속도로 바꿔 넣는다. 제동 시작 위치까지 가는 중이면 지금 속도를 유지한다.
void URemoteMovementComponent::UpdateControlledStop(float delta_seconds)
{
	if (!is_controlled_stop_ || delta_seconds <= 0.0f)
	{
		return;
	}
	APlayerCharacter* character = CastChecked<APlayerCharacter>(GetOwner());
	UCharacterMovementComponent* movement = character->GetCharacterMovement();
	if (is_stop_cruise_)
	{
		// 제동 시작 위치까지 이번 프레임에 다 가면, 보낸 사람 속도로 바꾸고 엔진 제동을 되살린다.
		const FVector current = character->GetActorLocation();
		const float to_start = FVector2D(stop_start_location_.X - current.X, stop_start_location_.Y - current.Y) | stop_direction_;
		const bool has_reached = to_start <= stop_speed_ * delta_seconds;
		const float speed = has_reached ? stop_sent_speed_ : stop_speed_;
		movement->Velocity.X = stop_direction_.X * speed;
		movement->Velocity.Y = stop_direction_.Y * speed;
		if (has_reached)
		{
			FinishControlledStop();
		}
		return;
	}
	const float start = stop_elapsed_;
	stop_elapsed_ = FMath::Min(stop_elapsed_ + delta_seconds, stop_duration_);
	const float deceleration = stop_speed_ * stop_speed_ / (2.0f * stop_distance_);
	const auto travelled = [this, deceleration](float time) { return stop_speed_ * time - 0.5f * deceleration * time * time; };
	const float speed = FMath::Max(0.0f, (travelled(stop_elapsed_) - travelled(start)) / delta_seconds);
	movement->Velocity.X = stop_direction_.X * speed;
	movement->Velocity.Y = stop_direction_.Y * speed;
	if (stop_elapsed_ >= stop_duration_)
	{
		FinishControlledStop();
	}
}

void URemoteMovementComponent::FinishControlledStop()
{
	UCharacterMovementComponent* movement = CastChecked<APlayerCharacter>(GetOwner())->GetCharacterMovement();
	movement->BrakingDecelerationWalking = saved_braking_deceleration_;
	movement->GroundFriction = saved_ground_friction_;
	is_controlled_stop_ = false;
	is_stop_cruise_ = false;
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
	else if (!is_controlled_stop_)
	{
		// 멈출 자리에 맞춰 세우는 동안에는 받은 위치로 보정하지 않는다. 다 선 뒤 받은 위치로 남은 차이를 맞춘다.
		correction_ = difference;
		correction_remaining_ = CORRECTION_SECONDS;
	}
	if (!has_state_)
	{
		character->SetActorRotation(FRotator(0.0f, state.yaw, 0.0f));
		view_pitch_ = state.view_pitch;
	}

	// 점프는 받은 속도로 시작하고, 착지는 걷기로 돌린다.
	const bool is_falling = state.air_phase != AirPhase::GROUND;
	if (is_falling != movement->IsFalling())
	{
		if (is_falling)
		{
			movement->Velocity = state.velocity;
		}
		movement->SetMovementMode(is_falling ? MOVE_Falling : MOVE_Walking);
	}

	state_ = state;
	has_state_ = true;
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

	UpdateControlledStop(delta_seconds);
	// 받은 입력으로 이동 계산을 이어 간다. 멈춤은 서버가 멈춤 패킷으로 알려 입력을 0으로 바꾼다.
	character->ApplyMovementInput(move_input_);
}
