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
	move_state.air_phase = state.air_phase;
	return move_state;
}

static FRemoteMoveState ToRemoteMoveState(const S2C_MOVE_PACKET& packet)
{
	FRemoteMoveState state;
	state.location = FVector(packet.location[0], packet.location[1], packet.location[2]);
	state.velocity = FVector(packet.velocity[0], packet.velocity[1], packet.velocity[2]);
	state.yaw = packet.yaw;
	state.view_pitch = packet.view_pitch;
	state.air_phase = packet.air_phase;
	return state;
}

// 패킷 구조체의 헤더 뒤 내용을 보낸다.
template<typename T>
static bool SendPacketBody(UGameNetworkSubsystem* network, uint8 type, const T& packet)
{
	TArray<uint8> payload;
	payload.Append(reinterpret_cast<const uint8*>(&packet) + sizeof(PACKET_HEADER), sizeof(packet) - sizeof(PACKET_HEADER));
	return network->SendPacket(type, payload);
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
	network->RegisterPacketHandler(S2C_MOVE_INPUT, FNetworkPacketHandler::CreateUObject(this, &UMovementSyncSubsystem::HandleMoveInputPacket));
	network->RegisterPacketHandler(S2C_MOVE_STOP, FNetworkPacketHandler::CreateUObject(this, &UMovementSyncSubsystem::HandleMoveStopPacket));
	network->RegisterPacketHandler(S2C_AIM, FNetworkPacketHandler::CreateUObject(this, &UMovementSyncSubsystem::HandleAimPacket));
	network->RegisterPacketHandler(S2C_EQUIP, FNetworkPacketHandler::CreateUObject(this, &UMovementSyncSubsystem::HandleEquipPacket));
	network->RegisterPacketHandler(S2C_ACTION, FNetworkPacketHandler::CreateUObject(this, &UMovementSyncSubsystem::HandleActionPacket));
	network->RegisterPacketHandler(S2C_RELOAD, FNetworkPacketHandler::CreateUObject(this, &UMovementSyncSubsystem::HandleReloadPacket));
	network->RegisterPacketHandler(S2C_HIT, FNetworkPacketHandler::CreateUObject(this, &UMovementSyncSubsystem::HandleHitPacket));
	network->RegisterPacketHandler(S2C_STUN_GAUGE, FNetworkPacketHandler::CreateUObject(this, &UMovementSyncSubsystem::HandleStunGaugePacket));
	network->RegisterPacketHandler(S2C_AMMO_USE, FNetworkPacketHandler::CreateUObject(this, &UMovementSyncSubsystem::HandleAmmoUsePacket));
	network->RegisterPacketHandler(S2C_AMMO_GAIN, FNetworkPacketHandler::CreateUObject(this, &UMovementSyncSubsystem::HandleAmmoGainPacket));
	network->RegisterPacketHandler(S2C_RELOAD_DONE, FNetworkPacketHandler::CreateUObject(this, &UMovementSyncSubsystem::HandleReloadDonePacket));
	network->RegisterPacketHandler(S2C_TIME_SYNC, FNetworkPacketHandler::CreateUObject(this, &UMovementSyncSubsystem::HandleTimeSyncPacket));
	network->RegisterPacketHandler(S2C_PICKAXE_CHARGES, FNetworkPacketHandler::CreateUObject(this, &UMovementSyncSubsystem::HandlePickaxeChargesPacket));
	network->RegisterPacketHandler(S2C_STUN_APPLY, FNetworkPacketHandler::CreateUObject(this, &UMovementSyncSubsystem::HandleStunApplyPacket));
	network->RegisterPacketHandler(S2C_STUN_RELEASE, FNetworkPacketHandler::CreateUObject(this, &UMovementSyncSubsystem::HandleStunReleasePacket));
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
			network->UnregisterPacketHandler(S2C_MOVE_INPUT);
			network->UnregisterPacketHandler(S2C_MOVE_STOP);
			network->UnregisterPacketHandler(S2C_AIM);
			network->UnregisterPacketHandler(S2C_EQUIP);
			network->UnregisterPacketHandler(S2C_ACTION);
			network->UnregisterPacketHandler(S2C_RELOAD);
			network->UnregisterPacketHandler(S2C_HIT);
			network->UnregisterPacketHandler(S2C_STUN_GAUGE);
			network->UnregisterPacketHandler(S2C_AMMO_USE);
			network->UnregisterPacketHandler(S2C_AMMO_GAIN);
			network->UnregisterPacketHandler(S2C_RELOAD_DONE);
			network->UnregisterPacketHandler(S2C_TIME_SYNC);
			network->UnregisterPacketHandler(S2C_PICKAXE_CHARGES);
			network->UnregisterPacketHandler(S2C_STUN_APPLY);
			network->UnregisterPacketHandler(S2C_STUN_RELEASE);
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
	SendInputChanges(*character);

	const UCharacterMovementComponent* movement = character->GetCharacterMovement();
	FRemoteMoveState state;
	state.location = character->GetActorLocation();
	state.velocity = character->GetVelocity();
	state.yaw = character->GetActorRotation().Yaw;
	state.view_pitch = FRotator::NormalizeAxis(character->GetViewRotation().Pitch);
	// 공중이면 수직 속도로 상승과 하강을 나눈다. 정점을 지나거나 천장에 부딪혀 위로 가는 속도가 없어지면 하강이 되어 그 순간 바로 보낸다.
	state.air_phase = !movement->IsFalling() ? AirPhase::GROUND : state.velocity.Z > 0.0 ? AirPhase::RISING : AirPhase::FALLING;

	const double now = GetWorld()->GetRealTimeSeconds();
	const double elapsed = now - last_send_time_;
	// 위치는 주기마다, 방향이 꺾이면 최소 간격을 두고 보내고, 공중 단계가 바뀌면 바로 보낸다.
	const bool is_air_phase_changed = has_sent_state_ && last_sent_state_.air_phase != state.air_phase;
	const bool is_periodic = elapsed >= MOVE_SEND_INTERVAL_MS / 1000.0;
	const bool is_direction_changed = elapsed >= MOVE_EXTRA_SEND_INTERVAL_MS / 1000.0 && HasDirectionChanged(last_sent_state_, state);
	if (has_sent_state_ && !is_forced && !is_air_phase_changed && !is_periodic && !is_direction_changed)
	{
		return;
	}
	MOVE_STATE move_state = ToMoveState(state);
	// 이 위치를 만든 게임 시각을 담는다. 서버는 상태 사이 시간을 이 차이로 재고, 서버 시각보다 빨리 흐르면 시간 조작으로 본다.
	move_state.client_time_ms = static_cast<uint32>(static_cast<uint64>(GetWorld()->GetTimeSeconds() * 1000.0));
	TArray<uint8> payload;
	payload.Append(reinterpret_cast<const uint8*>(&move_state), sizeof(move_state));
	if (GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>()->SendPacket(C2S_MOVE, payload))
	{
		last_sent_state_ = state;
		last_send_time_ = now;
		has_sent_state_ = true;
	}
}

// 이동 키 입력, 조준 여부, 손에 든 장비가 마지막으로 보낸 값과 다르면 바로 보낸다. 처음에는 모두 보낸다.
// 이동 키 입력은 이번 프레임에 넣은 값이고, 서버 속도 검증과 원격 이동 계산이 이 입력으로 앞뒤를 나눈다.
void UMovementSyncSubsystem::SendInputChanges(const APlayerCharacter& character)
{
	UGameNetworkSubsystem* network = GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>();
	const FVector2D move_input = character.GetMovementInput();
	C2S_MOVE_INPUT_PACKET input_packet{};
	input_packet.input_right = QuantizeInput(move_input.X);
	input_packet.input_forward = QuantizeInput(move_input.Y);
	if ((!has_sent_input_ || input_packet.input_right != sent_input_right_ || input_packet.input_forward != sent_input_forward_) && SendPacketBody(network, C2S_MOVE_INPUT, input_packet))
	{
		sent_input_right_ = input_packet.input_right;
		sent_input_forward_ = input_packet.input_forward;
	}
	C2S_AIM_PACKET aim_packet{};
	aim_packet.is_aiming = character.IsAiming();
	if ((!has_sent_input_ || aim_packet.is_aiming != is_sent_aiming_) && SendPacketBody(network, C2S_AIM, aim_packet))
	{
		is_sent_aiming_ = aim_packet.is_aiming;
	}
	const EWeaponType weapon = character.GetCurrentWeapon();
	C2S_EQUIP_PACKET equip_packet{};
	equip_packet.weapon = static_cast<uint8>(weapon);
	equip_packet.action_time_ms = GetActionTimeMs(character.GetEquipRealTime());
	if ((!has_sent_input_ || weapon != sent_weapon_) && SendPacketBody(network, C2S_EQUIP, equip_packet))
	{
		sent_weapon_ = weapon;
	}
	has_sent_input_ = true;
}

// 동작보다 먼저 바뀐 입력과 장비를 보내, 서버가 동작을 지금 손에 든 장비로 판정하게 한다.
// 총을 쏘면 서버가 발사 순간의 위치·방향으로 탄 광선을 판정하도록 지금 위치를 바로 보내고, 발사 패킷은 조준 방향이 정해진 뒤 HandleShotFired가 보낸다.
// 재장전은 손에 든 무기를 서버가 알고 있으므로 동작 시각만 보낸다.
void UMovementSyncSubsystem::HandleActionStarted(EPlayerAction action, EWeaponType weapon)
{
	if (const APlayerCharacter* character = local_character_.Get())
	{
		SendInputChanges(*character);
	}
	if (action == EPlayerAction::FIRE)
	{
		SendMoveState(true);
		return;
	}
	UGameNetworkSubsystem* network = GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>();
	// 캐릭터가 동작을 시작한 실제 시각으로 동작 시각을 만든다.
	const APlayerCharacter* character = local_character_.Get();
	const uint32 action_time_ms = GetActionTimeMs(character ? character->GetActionRealTime() : FPlatformTime::Seconds());
	if (action == EPlayerAction::RELOAD)
	{
		C2S_RELOAD_PACKET packet{};
		packet.action_time_ms = action_time_ms;
		SendPacketBody(network, C2S_RELOAD, packet);
		return;
	}
	C2S_ACTION_PACKET packet{};
	packet.action = static_cast<uint8>(action);
	packet.action_time_ms = action_time_ms;
	SendPacketBody(network, C2S_ACTION, packet);
}

uint32 UMovementSyncSubsystem::GetActionTimeMs(double real_time) const
{
	return sync_server_time_ms_ + static_cast<uint32>(static_cast<uint64>(FMath::Max(0.0, real_time - sync_real_time_) * 1000.0));
}

// 쏜 총의 퍼지기 전 조준 방향과 카메라 위치를 보낸다. 빗나감과 명중은 서버가 판정한다.
void UMovementSyncSubsystem::HandleShotFired(const FShotResult& shot)
{
	C2S_FIRE_PACKET packet{};
	for (int32 axis = 0; axis < 3; ++axis)
	{
		packet.aim_direction[axis] = static_cast<float>(shot.aim_direction[axis]);
		packet.camera[axis] = static_cast<float>(shot.camera_location[axis]);
	}
	// 캐릭터가 발사 간격을 센 것과 같은 실제 시각으로 동작 시각을 만든다.
	const APlayerCharacter* character = local_character_.Get();
	packet.action_time_ms = GetActionTimeMs(character ? character->GetActionRealTime() : FPlatformTime::Seconds());
	SendPacketBody(GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>(), C2S_FIRE, packet);
}

// 서버가 판정한 명중. 맞은 사람만 받는다. 쏜 사람 쪽으로 피격 방향을 표시한다.
void UMovementSyncSubsystem::HandleHitPacket(const TArray<uint8>& payload)
{
	if (payload.Num() != sizeof(S2C_HIT_PACKET) - sizeof(PACKET_HEADER))
	{
		HandleInvalidPacket();
		return;
	}
	S2C_HIT_PACKET packet{};
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
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

// 서버가 인정한 내 발사 뒤 그 무기의 남은 탄창 탄
void UMovementSyncSubsystem::HandleAmmoUsePacket(const TArray<uint8>& payload)
{
	if (payload.Num() != sizeof(S2C_AMMO_USE_PACKET) - sizeof(PACKET_HEADER))
	{
		HandleInvalidPacket();
		return;
	}
	S2C_AMMO_USE_PACKET packet{};
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	if (APlayerCharacter* character = local_character_.Get())
	{
		character->UseAmmo(static_cast<EWeaponType>(packet.weapon), packet.rounds);
	}
}

// 내가 연 탄약 상자에서 얻은 소지 탄 수
void UMovementSyncSubsystem::HandleAmmoGainPacket(const TArray<uint8>& payload)
{
	if (payload.Num() != sizeof(S2C_AMMO_GAIN_PACKET) - sizeof(PACKET_HEADER))
	{
		HandleInvalidPacket();
		return;
	}
	S2C_AMMO_GAIN_PACKET packet{};
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	if (APlayerCharacter* character = local_character_.Get())
	{
		character->GainAmmo(static_cast<EWeaponType>(packet.weapon), packet.rounds);
	}
}

// 서버의 재장전이 끝난 뒤 그 무기의 탄창 탄과 소지 탄
void UMovementSyncSubsystem::HandleReloadDonePacket(const TArray<uint8>& payload)
{
	if (payload.Num() != sizeof(S2C_RELOAD_DONE_PACKET) - sizeof(PACKET_HEADER))
	{
		HandleInvalidPacket();
		return;
	}
	S2C_RELOAD_DONE_PACKET packet{};
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	if (APlayerCharacter* character = local_character_.Get())
	{
		character->CompleteReload(static_cast<EWeaponType>(packet.weapon), packet.rounds, packet.spare_rounds);
	}
}

// 서버 시각. 게임 레벨에 들어올 때 한 번 온다. 받은 순간을 이 서버 시각으로 맞춘다.
void UMovementSyncSubsystem::HandleTimeSyncPacket(const TArray<uint8>& payload)
{
	if (payload.Num() != sizeof(S2C_TIME_SYNC_PACKET) - sizeof(PACKET_HEADER))
	{
		HandleInvalidPacket();
		return;
	}
	S2C_TIME_SYNC_PACKET packet{};
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	sync_server_time_ms_ = packet.server_time_ms;
	sync_real_time_ = FPlatformTime::Seconds();
}

// 서버가 곡괭이 휘두르기를 인정하거나 거부할 때, 1번이 다시 찰 때 보낸 사용 가능 횟수와 남은 충전 시간
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
void UMovementSyncSubsystem::HandleStunApplyPacket(const TArray<uint8>& payload)
{
	if (payload.Num() != sizeof(S2C_STUN_APPLY_PACKET) - sizeof(PACKET_HEADER))
	{
		HandleInvalidPacket();
		return;
	}
	S2C_STUN_APPLY_PACKET packet{};
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	const bool is_local = packet.player_id == GetWorld()->GetGameInstance()->GetSubsystem<UOnlineSessionSubsystem>()->GetPlayerKey();
	if (APlayerCharacter* character = is_local ? local_character_.Get() : FindRemotePlayer(packet.player_id))
	{
		character->StartStun();
	}
}

void UMovementSyncSubsystem::HandleStunReleasePacket(const TArray<uint8>& payload)
{
	if (payload.Num() != sizeof(S2C_STUN_RELEASE_PACKET) - sizeof(PACKET_HEADER))
	{
		HandleInvalidPacket();
		return;
	}
	S2C_STUN_RELEASE_PACKET packet{};
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	const bool is_local = packet.player_id == GetWorld()->GetGameInstance()->GetSubsystem<UOnlineSessionSubsystem>()->GetPlayerKey();
	if (APlayerCharacter* character = is_local ? local_character_.Get() : FindRemotePlayer(packet.player_id))
	{
		character->EndStun(packet.protection_ms / 1000.0f);
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
	if (payload.Num() != sizeof(S2C_MOVE_PACKET) - sizeof(PACKET_HEADER))
	{
		HandleInvalidPacket();
		return;
	}
	S2C_MOVE_PACKET packet{};
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	if (packet.player_id == GetWorld()->GetGameInstance()->GetSubsystem<UOnlineSessionSubsystem>()->GetPlayerKey())
	{
		return;
	}
	const FRemoteMoveState state = ToRemoteMoveState(packet);
	APlayerCharacter* remote = FindRemotePlayer(packet.player_id);
	if (!remote)
	{
		// 스폰 패킷보다 이동 패킷이 먼저 온 플레이어는 받은 위치에 만든다.
		remote = SpawnRemotePlayer(packet.player_id, FTransform(FRotator(0.0f, state.yaw, 0.0f), state.location));
	}
	if (URemoteMovementComponent* remote_movement = remote ? remote->FindComponentByClass<URemoteMovementComponent>() : nullptr)
	{
		remote_movement->ApplyState(state);
	}
}

// 다른 플레이어의 이동 키 입력·조준·장비. 아직 스폰되지 않았거나 내 것이면 버린다.
void UMovementSyncSubsystem::HandleMoveInputPacket(const TArray<uint8>& payload)
{
	if (payload.Num() != sizeof(S2C_MOVE_INPUT_PACKET) - sizeof(PACKET_HEADER))
	{
		HandleInvalidPacket();
		return;
	}
	S2C_MOVE_INPUT_PACKET packet{};
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	const APlayerCharacter* remote = FindRemotePlayer(packet.player_id);
	if (URemoteMovementComponent* remote_movement = remote ? remote->FindComponentByClass<URemoteMovementComponent>() : nullptr)
	{
		remote_movement->SetMoveInput(FVector2D(packet.input_right / 127.0, packet.input_forward / 127.0));
	}
}

// 서버가 그 플레이어를 멈춘 것으로 판단했다. 이동 키 입력을 없는 것으로 두어 제동으로 멈춘다.
void UMovementSyncSubsystem::HandleMoveStopPacket(const TArray<uint8>& payload)
{
	if (payload.Num() != sizeof(S2C_MOVE_STOP_PACKET) - sizeof(PACKET_HEADER))
	{
		HandleInvalidPacket();
		return;
	}
	S2C_MOVE_STOP_PACKET packet{};
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	const APlayerCharacter* remote = FindRemotePlayer(packet.player_id);
	if (URemoteMovementComponent* remote_movement = remote ? remote->FindComponentByClass<URemoteMovementComponent>() : nullptr)
	{
		remote_movement->SetMoveInput(FVector2D::ZeroVector);
	}
}

void UMovementSyncSubsystem::HandleAimPacket(const TArray<uint8>& payload)
{
	if (payload.Num() != sizeof(S2C_AIM_PACKET) - sizeof(PACKET_HEADER))
	{
		HandleInvalidPacket();
		return;
	}
	S2C_AIM_PACKET packet{};
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	const APlayerCharacter* remote = FindRemotePlayer(packet.player_id);
	if (URemoteMovementComponent* remote_movement = remote ? remote->FindComponentByClass<URemoteMovementComponent>() : nullptr)
	{
		remote_movement->SetAiming(packet.is_aiming);
	}
}

void UMovementSyncSubsystem::HandleEquipPacket(const TArray<uint8>& payload)
{
	if (payload.Num() != sizeof(S2C_EQUIP_PACKET) - sizeof(PACKET_HEADER))
	{
		HandleInvalidPacket();
		return;
	}
	S2C_EQUIP_PACKET packet{};
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	const APlayerCharacter* remote = FindRemotePlayer(packet.player_id);
	if (URemoteMovementComponent* remote_movement = remote ? remote->FindComponentByClass<URemoteMovementComponent>() : nullptr)
	{
		remote_movement->SetWeapon(ToWeaponType(packet.weapon));
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
	// 아직 스폰되지 않아 캐릭터가 없는 플레이어의 동작은 버린다. 동작은 손에 든 장비로 재생한다.
	if (APlayerCharacter* remote = FindRemotePlayer(packet.player_id))
	{
		remote->PlayRemoteAction(static_cast<EPlayerAction>(packet.action), remote->GetCurrentWeapon());
	}
}

void UMovementSyncSubsystem::HandleReloadPacket(const TArray<uint8>& payload)
{
	if (payload.Num() != sizeof(S2C_RELOAD_PACKET) - sizeof(PACKET_HEADER))
	{
		HandleInvalidPacket();
		return;
	}
	S2C_RELOAD_PACKET packet{};
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	// 내 재장전은 서버가 자동으로 시작했거나 내가 시작한 것을 인정한 것이다.
	if (packet.player_id == GetWorld()->GetGameInstance()->GetSubsystem<UOnlineSessionSubsystem>()->GetPlayerKey())
	{
		if (APlayerCharacter* character = local_character_.Get())
		{
			character->StartServerReload(ToWeaponType(packet.weapon));
		}
	}
	else if (APlayerCharacter* remote = FindRemotePlayer(packet.player_id))
	{
		remote->PlayRemoteAction(EPlayerAction::RELOAD, ToWeaponType(packet.weapon));
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
