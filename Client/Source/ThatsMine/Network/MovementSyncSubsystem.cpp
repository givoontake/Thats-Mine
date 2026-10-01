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
	return value <= static_cast<uint8>(EWeaponType::PISTOL) ? static_cast<EWeaponType>(value) : EWeaponType::PICKAXE;
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
	move_state.is_falling = state.is_falling;
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
	state.is_falling = move_state.is_falling;
	state.is_aiming = move_state.is_aiming;
	state.weapon = ToWeaponType(move_state.weapon);
	return state;
}

// 이동 키, 점프·착지, 조준, 무기가 바뀌었는지 확인한다. 입력 변화는 간격 제한 없이 즉시 보낸다.
static bool HasInputChanged(const FRemoteMoveState& previous, const FRemoteMoveState& current)
{
	return QuantizeInput(previous.move_input.X) != QuantizeInput(current.move_input.X) || QuantizeInput(previous.move_input.Y) != QuantizeInput(current.move_input.Y) || previous.is_falling != current.is_falling || previous.is_aiming != current.is_aiming || previous.weapon != current.weapon;
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
		}
		if (UOnlineSessionSubsystem* online = game_instance ? game_instance->GetSubsystem<UOnlineSessionSubsystem>() : nullptr)
		{
			online->on_changed_.RemoveAll(this);
		}
		if (APlayerCharacter* character = local_character_.Get())
		{
			character->on_action_started_.RemoveAll(this);
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
		SendMoveState();
	}
}

void UMovementSyncSubsystem::SendMoveState()
{
	const APlayerController* controller = GetWorld()->GetFirstPlayerController();
	APlayerCharacter* character = controller ? Cast<APlayerCharacter>(controller->GetPawn()) : nullptr;
	if (character != local_character_.Get())
	{
		if (APlayerCharacter* previous = local_character_.Get())
		{
			previous->on_action_started_.RemoveAll(this);
		}
		local_character_ = character;
		if (character)
		{
			character->on_action_started_.AddUObject(this, &UMovementSyncSubsystem::HandleActionStarted);
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
	// 이동 계산에 들어간 가속 방향을 몸 기준으로 바꿔 이동 입력으로 쓴다.
	const FVector input = character->GetActorQuat().UnrotateVector(movement->GetCurrentAcceleration() / FMath::Max(movement->GetMaxAcceleration(), UE_KINDA_SMALL_NUMBER));
	state.move_input = FVector2D(input.Y, input.X);
	state.is_falling = movement->IsFalling();
	state.is_aiming = character->IsAiming();
	state.weapon = character->GetCurrentWeapon();

	const double now = GetWorld()->GetRealTimeSeconds();
	const double elapsed = now - last_send_time_;
	// 입력 변화는 즉시, 위치는 주기마다, 방향이 꺾이면 최소 간격을 두고 보낸다.
	const bool is_input_changed = has_sent_state_ && HasInputChanged(last_sent_state_, state);
	const bool is_periodic = elapsed >= MOVE_SEND_INTERVAL_MS / 1000.0;
	const bool is_direction_changed = elapsed >= MOVE_EXTRA_SEND_INTERVAL_MS / 1000.0 && HasDirectionChanged(last_sent_state_, state);
	if (has_sent_state_ && !is_input_changed && !is_periodic && !is_direction_changed)
	{
		return;
	}
	const MOVE_STATE move_state = ToMoveState(state);
	TArray<uint8> payload;
	payload.Append(reinterpret_cast<const uint8*>(&move_state), sizeof(move_state));
	if (GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>()->SendPacket(is_input_changed ? C2S_MOVE_INPUT : C2S_MOVE, payload))
	{
		last_sent_state_ = state;
		last_send_time_ = now;
		has_sent_state_ = true;
	}
}

void UMovementSyncSubsystem::HandleActionStarted(EPlayerAction action, EWeaponType weapon)
{
	C2S_ACTION_PACKET packet{};
	packet.action = static_cast<uint8>(action);
	packet.weapon = static_cast<uint8>(weapon);
	TArray<uint8> payload;
	payload.Append(reinterpret_cast<const uint8*>(&packet) + sizeof(PACKET_HEADER), sizeof(packet) - sizeof(PACKET_HEADER));
	GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>()->SendPacket(C2S_ACTION, payload);
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
