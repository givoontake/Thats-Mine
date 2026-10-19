#include "MovementSyncSubsystem.h"

#include <cstddef>
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Flow/ClientStateSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameNetworkSubsystem.h"
#include "OnlineSessionSubsystem.h"
#include "Player/PlayerCharacter.h"
#include "Player/RemoteMovementComponent.h"
#include "game_protocol.h"

DEFINE_LOG_CATEGORY_STATIC(LogMovementSync, Log, All);

// 이동 방향이 마지막으로 보낸 값보다 이만큼 꺾이면 주기보다 먼저 보낸다.
static constexpr float DIRECTION_CHANGE_DEGREES = 15.0f;
// 이보다 느리면 이동 방향을 비교하지 않는다.
static constexpr float MIN_MOVING_SPEED = 10.0f;

static int8 QuantizeInput(double value)
{
	return static_cast<int8>(FMath::RoundToInt(FMath::Clamp(value, -1.0, 1.0) * 127.0));
}

static EWeaponType ToWeaponType(uint8 value)
{
	return value <= static_cast<uint8>(EWeaponType::MAGNIFIER) ? static_cast<EWeaponType>(value) : EWeaponType::PICKAXE;
}

static MOVE_STATE ToMoveState(const FRemoteMoveState& state)
{
	MOVE_STATE move_state{};
	move_state.location[0] = static_cast<float>(state.location.X);
	move_state.location[1] = static_cast<float>(state.location.Y);
	move_state.location[2] = static_cast<float>(state.location.Z);
	move_state.velocity[0] = static_cast<float>(state.velocity.X);
	move_state.velocity[1] = static_cast<float>(state.velocity.Y);
	move_state.velocity[2] = static_cast<float>(state.velocity.Z);
	move_state.yaw = state.yaw;
	move_state.view_pitch = state.view_pitch;
	move_state.input_right = QuantizeInput(state.move_input.X);
	move_state.input_forward = QuantizeInput(state.move_input.Y);
	move_state.air_phase = state.air_phase;
	move_state.is_aiming = state.is_aiming;
	move_state.weapon = static_cast<uint8>(state.weapon);
	return move_state;
}

static FRemoteMoveState ToRemoteMoveState(const MOVE_STATE& move_state)
{
	FRemoteMoveState state;
	state.location = FVector(move_state.location[0], move_state.location[1], move_state.location[2]);
	state.velocity = FVector(move_state.velocity[0], move_state.velocity[1], move_state.velocity[2]);
	state.yaw = move_state.yaw;
	state.view_pitch = move_state.view_pitch;
	state.move_input = FVector2D(move_state.input_right / 127.0, move_state.input_forward / 127.0);
	state.air_phase = move_state.air_phase;
	state.is_aiming = move_state.is_aiming;
	state.weapon = ToWeaponType(move_state.weapon);
	return state;
}

// 이동 키, 공중 단계(점프·하강·착지), 조준, 무기가 바뀌었는지 확인한다. 입력 변화는 간격 제한 없이 즉시 보낸다.
static bool HasInputChanged(const FRemoteMoveState& previous, const FRemoteMoveState& current)
{
	return QuantizeInput(previous.move_input.X) != QuantizeInput(current.move_input.X) || QuantizeInput(previous.move_input.Y) != QuantizeInput(current.move_input.Y) || previous.air_phase != current.air_phase || previous.is_aiming != current.is_aiming || previous.weapon != current.weapon;
}

// 마우스 회전 등으로 이동 방향이 기준 이상 꺾였는지 확인한다.
static bool HasDirectionChanged(const FRemoteMoveState& previous, const FRemoteMoveState& current)
{
	const FVector2D previous_velocity(previous.velocity);
	const FVector2D current_velocity(current.velocity);
	if (previous_velocity.Size() < MIN_MOVING_SPEED || current_velocity.Size() < MIN_MOVING_SPEED)
	{
		return false;
	}
	return FVector2D::DotProduct(previous_velocity.GetSafeNormal(), current_velocity.GetSafeNormal()) < FMath::Cos(FMath::DegreesToRadians(DIRECTION_CHANGE_DEGREES));
}

void UMovementSyncSubsystem::OnWorldBeginPlay(UWorld& world)
{
	Super::OnWorldBeginPlay(world);
	UGameInstance* game_instance = world.GetGameInstance();
	const UClientStateSubsystem* flow = game_instance ? game_instance->GetSubsystem<UClientStateSubsystem>() : nullptr;
	// 온라인 게임 레벨에서만 동작한다. 게임 레벨의 월드는 로딩 상태에서 시작된다.
	if (!flow || flow->IsLocalMode() || (flow->GetState() != EClientState::LOADING && flow->GetState() != EClientState::PLAYING))
	{
		return;
	}
	UGameNetworkSubsystem* network = game_instance->GetSubsystem<UGameNetworkSubsystem>();
	is_active_ = network->RegisterPacketHandler(S2C_MOVE, FNetworkPacketHandler::CreateUObject(this, &UMovementSyncSubsystem::HandleMovePacket));
	if (!is_active_)
	{
		return;
	}
	network->RegisterPacketHandler(S2C_ACTION, FNetworkPacketHandler::CreateUObject(this, &UMovementSyncSubsystem::HandleActionPacket));
	network->RegisterPacketHandler(S2C_HIT, FNetworkPacketHandler::CreateUObject(this, &UMovementSyncSubsystem::HandleHitPacket));
	network->RegisterPacketHandler(S2C_STUN_GAUGE, FNetworkPacketHandler::CreateUObject(this, &UMovementSyncSubsystem::HandleStunGaugePacket));
	network->RegisterPacketHandler(S2C_AMMO, FNetworkPacketHandler::CreateUObject(this, &UMovementSyncSubsystem::HandleAmmoPacket));
	network->RegisterPacketHandler(S2C_SHOT_SEED, FNetworkPacketHandler::CreateUObject(this, &UMovementSyncSubsystem::HandleShotSeedPacket));
	network->RegisterPacketHandler(S2C_FIRE_REJECTED, FNetworkPacketHandler::CreateUObject(this, &UMovementSyncSubsystem::HandleFireRejectedPacket));
	network->RegisterPacketHandler(S2C_SHOT_TRACE, FNetworkPacketHandler::CreateUObject(this, &UMovementSyncSubsystem::HandleShotTracePacket));
	network->RegisterPacketHandler(S2C_PICKAXE_CHARGES, FNetworkPacketHandler::CreateUObject(this, &UMovementSyncSubsystem::HandlePickaxeChargesPacket));
	network->RegisterPacketHandler(S2C_STUN, FNetworkPacketHandler::CreateUObject(this, &UMovementSyncSubsystem::HandleStunPacket));
	network->RegisterPacketHandler(S2C_MOVE_CORRECTION, FNetworkPacketHandler::CreateUObject(this, &UMovementSyncSubsystem::HandleMoveCorrectionPacket));
	game_instance->GetSubsystem<UOnlineSessionSubsystem>()->on_changed_.AddUObject(this, &UMovementSyncSubsystem::RemoveLeftPlayers);
}

void UMovementSyncSubsystem::Deinitialize()
{
	if (is_active_)
	{
		// 게임을 종료할 때는 게임 인스턴스의 서브시스템이 월드보다 먼저 정리되어 없을 수 있다.
		UGameInstance* game_instance = GetWorld()->GetGameInstance();
		if (UGameNetworkSubsystem* network = game_instance ? game_instance->GetSubsystem<UGameNetworkSubsystem>() : nullptr)
		{
			network->UnregisterPacketHandler(S2C_MOVE);
			network->UnregisterPacketHandler(S2C_ACTION);
			network->UnregisterPacketHandler(S2C_HIT);
			network->UnregisterPacketHandler(S2C_STUN_GAUGE);
			network->UnregisterPacketHandler(S2C_AMMO);
			network->UnregisterPacketHandler(S2C_SHOT_SEED);
			network->UnregisterPacketHandler(S2C_FIRE_REJECTED);
			network->UnregisterPacketHandler(S2C_SHOT_TRACE);
			network->UnregisterPacketHandler(S2C_PICKAXE_CHARGES);
			network->UnregisterPacketHandler(S2C_STUN);
			network->UnregisterPacketHandler(S2C_MOVE_CORRECTION);
		}
		if (UOnlineSessionSubsystem* online = game_instance ? game_instance->GetSubsystem<UOnlineSessionSubsystem>() : nullptr)
		{
			online->on_changed_.RemoveAll(this);
		}
		if (APlayerCharacter* character = local_character_.Get())
		{
			character->on_action_started_.RemoveAll(this);
			character->on_shot_fired_.RemoveDynamic(this, &UMovementSyncSubsystem::HandleShotFired);
		}
		is_active_ = false;
	}
	remote_players_.Reset();
	Super::Deinitialize();
}

bool UMovementSyncSubsystem::DoesSupportWorldType(const EWorldType::Type world_type) const
{
	return world_type == EWorldType::Game || world_type == EWorldType::PIE;
}

bool UMovementSyncSubsystem::IsTickable() const
{
	return is_active_;
}

TStatId UMovementSyncSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMovementSyncSubsystem, STATGROUP_Tickables);
}

void UMovementSyncSubsystem::Tick(float delta_seconds)
{
	Super::Tick(delta_seconds);
	if (GetWorld()->GetGameInstance()->GetSubsystem<UClientStateSubsystem>()->GetState() == EClientState::PLAYING)
	{
		SendMoveState(false);
	}
}

void UMovementSyncSubsystem::SendMoveState(bool is_forced)
{
	const APlayerController* controller = GetWorld()->GetFirstPlayerController();
	APlayerCharacter* character = controller ? Cast<APlayerCharacter>(controller->GetPawn()) : nullptr;
	if (character != local_character_.Get())
	{
		if (APlayerCharacter* previous = local_character_.Get())
		{
			previous->on_action_started_.RemoveAll(this);
			previous->on_shot_fired_.RemoveDynamic(this, &UMovementSyncSubsystem::HandleShotFired);
		}
		local_character_ = character;
		if (character)
		{
			character->on_action_started_.AddUObject(this, &UMovementSyncSubsystem::HandleActionStarted);
			character->on_shot_fired_.AddDynamic(this, &UMovementSyncSubsystem::HandleShotFired);
		}
	}
	if (!character)
	{
		return;
	}

	const UCharacterMovementComponent* movement = character->GetCharacterMovement();
	FRemoteMoveState state;
	state.location = character->GetActorLocation();
	state.velocity = character->GetVelocity();
	state.yaw = character->GetActorRotation().Yaw;
	state.view_pitch = FRotator::NormalizeAxis(character->GetViewRotation().Pitch);
	// 이번 프레임에 넣은 이동 키 입력을 보낸다. 서버 속도 검증과 원격 이동 계산이 키 입력 기준으로 앞뒤를 나눈다.
	state.move_input = character->GetMovementInput();
	// 공중이면 수직 속도로 상승과 하강을 나눈다. 정점을 지나거나 천장에 부딪혀 위로 가는 속도가 없어지면 하강이 되어 그 순간 바로 보낸다.
	state.air_phase = !movement->IsFalling() ? AirPhase::GROUND : state.velocity.Z > 0.0 ? AirPhase::RISING : AirPhase::FALLING;
	state.is_aiming = character->IsAiming();
	state.weapon = character->GetCurrentWeapon();

	const double now = GetWorld()->GetRealTimeSeconds();
	const double elapsed = now - last_send_time_;
	// 입력 변화는 즉시, 위치는 주기마다, 방향이 꺾이면 최소 간격을 두고 보낸다.
	const bool is_input_changed = is_forced || (has_sent_state_ && HasInputChanged(last_sent_state_, state));
	const bool is_periodic = elapsed >= MOVE_SEND_INTERVAL_MS / 1000.0;
	const bool is_direction_changed = elapsed >= MOVE_EXTRA_SEND_INTERVAL_MS / 1000.0 && HasDirectionChanged(last_sent_state_, state);
	if (has_sent_state_ && !is_input_changed && !is_periodic && !is_direction_changed)
	{
		return;
	}
	MOVE_STATE move_state = ToMoveState(state);
	// 이 위치를 만든 게임 시각을 담는다. 서버는 상태 사이 시간을 이 차이로 재고, 서버 시각보다 빨리 흐르면 시간 조작으로 본다.
	move_state.client_time_ms = static_cast<uint32>(static_cast<uint64>(GetWorld()->GetTimeSeconds() * 1000.0));
	TArray<uint8> payload;
	payload.Append(reinterpret_cast<const uint8*>(&move_state), sizeof(move_state));
	if (GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>()->SendPacket(is_input_changed ? C2S_MOVE_INPUT : C2S_MOVE, payload))
	{
		last_sent_state_ = state;
		last_send_time_ = now;
		has_sent_state_ = true;
	}
}

// 총을 쏘면 서버가 발사 순간의 위치·방향으로 총구와 조준을 판정하도록, 동작보다 먼저 지금 이동 상태를 바로 보낸다.
void UMovementSyncSubsystem::HandleActionStarted(EPlayerAction action, EWeaponType weapon)
{
	if (action == EPlayerAction::FIRE)
	{
		SendMoveState(true);
	}
	C2S_ACTION_PACKET packet{};
	packet.action = static_cast<uint8>(action);
	packet.weapon = static_cast<uint8>(weapon);
	TArray<uint8> payload;
	payload.Append(reinterpret_cast<const uint8*>(&packet) + sizeof(PACKET_HEADER), sizeof(packet) - sizeof(PACKET_HEADER));
	GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>()->SendPacket(C2S_ACTION, payload);
}

// 맞은 뼈를 서버 판정 데이터와 같은 부위 값으로 바꾼다.
static HitboxPart GetHitboxPart(const FName& bone)
{
	const FString name = bone.ToString();
	if (name == TEXT("head"))
	{
		return HitboxPart::HEAD;
	}
	if (name.StartsWith(TEXT("upperarm")) || name.StartsWith(TEXT("lowerarm")))
	{
		return HitboxPart::ARM;
	}
	if (name.StartsWith(TEXT("hand")))
	{
		return HitboxPart::HAND;
	}
	if (name.StartsWith(TEXT("upperleg")) || name.StartsWith(TEXT("lowerleg")))
	{
		return HitboxPart::LEG;
	}
	if (name.StartsWith(TEXT("foot")))
	{
		return HitboxPart::FOOT;
	}
	return HitboxPart::TORSO;
}

// 발사마다 탄이 끝난 곳을 보내 다른 플레이어 화면에 궤적을 그리게 한다.
// 총알이 다른 플레이어 메시의 히트박스에 맞았을 때만 명중을 보낸다. 판정 광선의 시작점·방향·맞은 곳까지 거리와 맞은 부위를 담는다.
void UMovementSyncSubsystem::HandleShotFired(const FShotResult& shot)
{
	C2S_SHOT_TRACE_PACKET trace_packet{};
	trace_packet.weapon = static_cast<uint8>(shot.weapon);
	for (int32 axis = 0; axis < 3; ++axis)
	{
		trace_packet.end[axis] = static_cast<float>(shot.trace_end[axis]);
	}
	TArray<uint8> trace_payload;
	trace_payload.Append(reinterpret_cast<const uint8*>(&trace_packet) + sizeof(PACKET_HEADER), sizeof(trace_packet) - sizeof(PACKET_HEADER));
	GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>()->SendPacket(C2S_SHOT_TRACE, trace_payload);

	const APlayerCharacter* target = Cast<APlayerCharacter>(shot.hit.GetActor());
	if (!shot.has_hit || !target || shot.hit.GetComponent() != target->GetMesh())
	{
		return;
	}
	const int32* target_player_id = remote_players_.FindKey(TWeakObjectPtr<APlayerCharacter>(const_cast<APlayerCharacter*>(target)));
	if (!target_player_id)
	{
		return;
	}
	const FVector ray = shot.hit.ImpactPoint - shot.trace_start;
	C2S_HIT_PACKET packet{};
	packet.target_player_id = *target_player_id;
	packet.weapon = static_cast<uint8>(shot.weapon);
	packet.part = static_cast<uint8>(GetHitboxPart(shot.hit.BoneName));
	for (int32 axis = 0; axis < 3; ++axis)
	{
		packet.camera[axis] = static_cast<float>(shot.camera_location[axis]);
		packet.direction[axis] = static_cast<float>(shot.shot_direction[axis]);
	}
	packet.distance = static_cast<float>(ray.Size());
	for (int32 axis = 0; axis < 3; ++axis)
	{
		packet.aim_direction[axis] = static_cast<float>(shot.aim_direction[axis]);
	}
	packet.spread = shot.spread;
	TArray<uint8> payload;
	payload.Append(reinterpret_cast<const uint8*>(&packet) + sizeof(PACKET_HEADER), sizeof(packet) - sizeof(PACKET_HEADER));
	GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>()->SendPacket(C2S_HIT, payload);
}

// 서버가 인정한 명중. 쏜 사람과 맞은 사람만 받는다. 내가 맞았으면 쏜 사람 쪽으로 피격 방향을 표시한다.
void UMovementSyncSubsystem::HandleHitPacket(const TArray<uint8>& payload)
{
	if (payload.Num() != sizeof(S2C_HIT_PACKET) - sizeof(PACKET_HEADER))
	{
		HandleInvalidPacket();
		return;
	}
	S2C_HIT_PACKET packet{};
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	UE_LOG(LogMovementSync, Log, TEXT("Hit confirmed: shooter=%d target=%d part=%d"), packet.shooter_player_id, packet.target_player_id, packet.part);
	if (packet.target_player_id != GetWorld()->GetGameInstance()->GetSubsystem<UOnlineSessionSubsystem>()->GetPlayerKey())
	{
		return;
	}
	APlayerCharacter* character = local_character_.Get();
	const APlayerCharacter* shooter = FindRemotePlayer(packet.shooter_player_id);
	if (character && shooter)
	{
		character->ShowDamageDirection(shooter->GetActorLocation());
	}
}

// 내 기절 게이지가 바뀌었다. 이후 감소는 캐릭터가 계산해 표시한다.
void UMovementSyncSubsystem::HandleStunGaugePacket(const TArray<uint8>& payload)
{
	if (payload.Num() != sizeof(S2C_STUN_GAUGE_PACKET) - sizeof(PACKET_HEADER))
	{
		HandleInvalidPacket();
		return;
	}
	S2C_STUN_GAUGE_PACKET packet{};
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	if (APlayerCharacter* character = local_character_.Get())
	{
		character->SetStunGauge(packet.gauge);
	}
}

// 서버가 내 발사나 재장전을 인정하지 않았거나 내가 탄약 상자를 열어 보낸 그 무기의 탄약 상태
void UMovementSyncSubsystem::HandleAmmoPacket(const TArray<uint8>& payload)
{
	if (payload.Num() != sizeof(S2C_AMMO_PACKET) - sizeof(PACKET_HEADER))
	{
		HandleInvalidPacket();
		return;
	}
	S2C_AMMO_PACKET packet{};
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	if (APlayerCharacter* character = local_character_.Get())
	{
		character->SetAmmo(static_cast<EWeaponType>(packet.weapon), packet.rounds, packet.spare_rounds);
	}
}

// 이번 게임의 탄 퍼짐 시드. 게임 레벨에 들어올 때 탑 배치 뒤에 온다.
void UMovementSyncSubsystem::HandleShotSeedPacket(const TArray<uint8>& payload)
{
	if (payload.Num() != sizeof(S2C_SHOT_SEED_PACKET) - sizeof(PACKET_HEADER))
	{
		HandleInvalidPacket();
		return;
	}
	S2C_SHOT_SEED_PACKET packet{};
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	shot_seed_ = packet.seed;
}

// 다른 플레이어의 발사 궤적. 그 캐릭터의 총구에서 탄이 끝난 곳까지 그린다.
void UMovementSyncSubsystem::HandleShotTracePacket(const TArray<uint8>& payload)
{
	if (payload.Num() != sizeof(S2C_SHOT_TRACE_PACKET) - sizeof(PACKET_HEADER))
	{
		HandleInvalidPacket();
		return;
	}
	S2C_SHOT_TRACE_PACKET packet{};
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	if (APlayerCharacter* shooter = FindRemotePlayer(packet.player_id))
	{
		shooter->ShowShotTracer(FVector(packet.end[0], packet.end[1], packet.end[2]));
	}
}

// 서버가 인정하지 않은 내 총 발사. 그 무기의 발사 번호를 되돌려 서버와 맞춘다.
void UMovementSyncSubsystem::HandleFireRejectedPacket(const TArray<uint8>& payload)
{
	if (payload.Num() != sizeof(S2C_FIRE_REJECTED_PACKET) - sizeof(PACKET_HEADER))
	{
		HandleInvalidPacket();
		return;
	}
	S2C_FIRE_REJECTED_PACKET packet{};
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	if (APlayerCharacter* character = local_character_.Get())
	{
		character->CancelShot(static_cast<EWeaponType>(packet.weapon));
	}
}

// 서버가 내 곡괭이 휘두르기를 인정하지 않아 보낸 사용 가능 횟수와 남은 충전 시간
void UMovementSyncSubsystem::HandlePickaxeChargesPacket(const TArray<uint8>& payload)
{
	if (payload.Num() != sizeof(S2C_PICKAXE_CHARGES_PACKET) - sizeof(PACKET_HEADER))
	{
		HandleInvalidPacket();
		return;
	}
	S2C_PICKAXE_CHARGES_PACKET packet{};
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	if (APlayerCharacter* character = local_character_.Get())
	{
		character->SetPickaxeCharges(packet.charges, static_cast<int32>(packet.remaining_ms));
	}
}

// 나 또는 다른 플레이어가 기절했다. 기절이 끝나면 받은 보호 시간 동안 보호된다.
void UMovementSyncSubsystem::HandleStunPacket(const TArray<uint8>& payload)
{
	if (payload.Num() != sizeof(S2C_STUN_PACKET) - sizeof(PACKET_HEADER))
	{
		HandleInvalidPacket();
		return;
	}
	S2C_STUN_PACKET packet{};
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	const bool is_local = packet.player_id == GetWorld()->GetGameInstance()->GetSubsystem<UOnlineSessionSubsystem>()->GetPlayerKey();
	if (APlayerCharacter* character = is_local ? local_character_.Get() : FindRemotePlayer(packet.player_id))
	{
		character->StartStun(packet.duration_ms / 1000.0f, packet.protection_ms / 1000.0f);
	}
}

// 서버가 내 이동을 인정하지 않았다. 받은 위치로 바로 옮겨 수평으로 멈추고, 공중이면 받은 수직 속도로 이어 떨어진다.
// 같은 번호로 확인을 보낸 뒤 다음 틱에 새 위치를 바로 보낸다.
void UMovementSyncSubsystem::HandleMoveCorrectionPacket(const TArray<uint8>& payload)
{
	if (payload.Num() != sizeof(S2C_MOVE_CORRECTION_PACKET) - sizeof(PACKET_HEADER))
	{
		HandleInvalidPacket();
		return;
	}
	S2C_MOVE_CORRECTION_PACKET packet{};
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	if (APlayerCharacter* character = local_character_.Get())
	{
		character->SetActorLocation(FVector(packet.location[0], packet.location[1], packet.location[2]), false, nullptr, ETeleportType::TeleportPhysics);
		UCharacterMovementComponent* movement = character->GetCharacterMovement();
		movement->StopMovementImmediately();
		if (packet.air_phase != AirPhase::GROUND)
		{
			movement->SetMovementMode(MOVE_Falling);
			movement->Velocity = FVector(0.0, 0.0, packet.velocity_z);
		}
	}
	C2S_MOVE_CORRECTION_ACK_PACKET ack{};
	ack.correction_id = packet.correction_id;
	TArray<uint8> ack_payload;
	ack_payload.Append(reinterpret_cast<const uint8*>(&ack) + sizeof(PACKET_HEADER), sizeof(ack) - sizeof(PACKET_HEADER));
	GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>()->SendPacket(C2S_MOVE_CORRECTION_ACK, ack_payload);
	has_sent_state_ = false;
}

void UMovementSyncSubsystem::HandleMovePacket(const TArray<uint8>& payload)
{
	constexpr int32 ENTRIES_OFFSET = static_cast<int32>(offsetof(S2C_MOVE_PACKET, entries) - sizeof(PACKET_HEADER));
	if (payload.Num() < ENTRIES_OFFSET)
	{
		HandleInvalidPacket();
		return;
	}
	const int32 player_count = payload[0];
	if (player_count > MAX_ROOM_PLAYER_COUNT || payload.Num() != ENTRIES_OFFSET + player_count * static_cast<int32>(sizeof(S2C_MOVE_ENTRY)))
	{
		HandleInvalidPacket();
		return;
	}
	const int64 local_player_key = GetWorld()->GetGameInstance()->GetSubsystem<UOnlineSessionSubsystem>()->GetPlayerKey();
	for (int32 index = 0; index < player_count; ++index)
	{
		S2C_MOVE_ENTRY entry;
		FMemory::Memcpy(&entry, payload.GetData() + ENTRIES_OFFSET + index * sizeof(S2C_MOVE_ENTRY), sizeof(entry));
		if (entry.player_id == local_player_key)
		{
			continue;
		}
		const FRemoteMoveState state = ToRemoteMoveState(entry.move_state);
		APlayerCharacter* remote = FindRemotePlayer(entry.player_id);
		if (!remote)
		{
			// 스폰 패킷보다 이동 패킷이 먼저 온 플레이어는 받은 위치에 만든다.
			remote = SpawnRemotePlayer(entry.player_id, FTransform(FRotator(0.0f, state.yaw, 0.0f), state.location));
		}
		if (URemoteMovementComponent* remote_movement = remote ? remote->FindComponentByClass<URemoteMovementComponent>() : nullptr)
		{
			remote_movement->ApplyState(state);
		}
	}
}

void UMovementSyncSubsystem::HandleActionPacket(const TArray<uint8>& payload)
{
	if (payload.Num() != sizeof(S2C_ACTION_PACKET) - sizeof(PACKET_HEADER))
	{
		HandleInvalidPacket();
		return;
	}
	S2C_ACTION_PACKET packet{};
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	if (packet.action > static_cast<uint8>(EPlayerAction::PICKUP))
	{
		return;
	}
	// 아직 스폰되지 않아 캐릭터가 없는 플레이어의 동작은 버린다.
	if (APlayerCharacter* remote = FindRemotePlayer(packet.player_id))
	{
		remote->PlayRemoteAction(static_cast<EPlayerAction>(packet.action), ToWeaponType(packet.weapon));
	}
}

void UMovementSyncSubsystem::RemoveLeftPlayers()
{
	const TArray<FRoomMemberInfo>& members = GetWorld()->GetGameInstance()->GetSubsystem<UOnlineSessionSubsystem>()->GetMembers();
	for (auto it = remote_players_.CreateIterator(); it; ++it)
	{
		const int32 player_id = it.Key();
		if (members.ContainsByPredicate([player_id](const FRoomMemberInfo& member) { return member.player_key == player_id; }))
		{
			continue;
		}
		if (APlayerCharacter* remote = it.Value().Get())
		{
			remote->Destroy();
		}
		it.RemoveCurrent();
	}
}

void UMovementSyncSubsystem::HandleInvalidPacket()
{
	UE_LOG(LogMovementSync, Warning, TEXT("Invalid movement sync packet."));
	GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>()->Disconnect();
}

APlayerCharacter* UMovementSyncSubsystem::FindRemotePlayer(int32 player_id) const
{
	const TWeakObjectPtr<APlayerCharacter>* remote = remote_players_.Find(player_id);
	return remote ? remote->Get() : nullptr;
}

APlayerCharacter* UMovementSyncSubsystem::SpawnRemotePlayer(int32 player_id, const FTransform& transform)
{
	if (APlayerCharacter* remote = FindRemotePlayer(player_id))
	{
		return remote;
	}
	const AGameModeBase* game_mode = GetWorld()->GetAuthGameMode();
	if (!game_mode || !game_mode->DefaultPawnClass)
	{
		return nullptr;
	}
	// 내 캐릭터와 같은 클래스로 만들되 조종자 없이 둔다.
	APlayerCharacter* remote = GetWorld()->SpawnActorDeferred<APlayerCharacter>(game_mode->DefaultPawnClass, transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!remote)
	{
		return nullptr;
	}
	remote->AutoPossessPlayer = EAutoReceiveInput::Disabled;
	remote->AutoPossessAI = EAutoPossessAI::Disabled;
	remote->FinishSpawning(transform);
	remote->AddComponentByClass(URemoteMovementComponent::StaticClass(), false, FTransform::Identity, false);
	remote_players_.Add(player_id, remote);
	return remote;
}
