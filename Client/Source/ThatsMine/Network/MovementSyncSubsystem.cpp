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
#include "Player/PlayerMovementComponent.h"
#include "Player/RemoteMovementComponent.h"
#include "game_protocol.h"
#include "movement_settings.h"

DEFINE_LOG_CATEGORY_STATIC(LogMovementSync, Log, All);

static EWeaponType ToWeaponType(uint8 value)
{
	return value <= static_cast<uint8>(EWeaponType::MAGNIFIER) ? static_cast<EWeaponType>(value) : EWeaponType::PICKAXE;
}

// 이동 상태 칸이 같은 패킷(이동 중계, 롤백)에서 이동 상태를 꺼낸다.
template<typename T>
static FRemoteMoveState ToRemoteMoveState(const T& packet)
{
	FRemoteMoveState state;
	state.location = FVector(packet.location[0], packet.location[1], packet.location[2]);
	state.velocity = FVector(packet.velocity[0], packet.velocity[1], packet.velocity[2]);
	state.yaw = packet.yaw;
	state.view_pitch = packet.view_pitch;
	state.is_falling = packet.is_falling;
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

// 패킷 구조체의 헤더 뒤 내용
template<typename T>
static TArray<uint8> GetPacketBody(const T& packet)
{
	TArray<uint8> payload;
	payload.Append(reinterpret_cast<const uint8*>(&packet) + sizeof(PACKET_HEADER), sizeof(packet) - sizeof(PACKET_HEADER));
	return payload;
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
	network->RegisterPacketHandler(S2C_PICKAXE_CHARGES, FNetworkPacketHandler::CreateUObject(this, &UMovementSyncSubsystem::HandlePickaxeChargesPacket));
	network->RegisterPacketHandler(S2C_STUN_APPLY, FNetworkPacketHandler::CreateUObject(this, &UMovementSyncSubsystem::HandleStunApplyPacket));
	network->RegisterPacketHandler(S2C_STUN_RELEASE, FNetworkPacketHandler::CreateUObject(this, &UMovementSyncSubsystem::HandleStunReleasePacket));
	network->RegisterPacketHandler(S2C_MOVE_ROLLBACK, FNetworkPacketHandler::CreateUObject(this, &UMovementSyncSubsystem::HandleMoveRollbackPacket));
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
			network->UnregisterPacketHandler(S2C_PICKAXE_CHARGES);
			network->UnregisterPacketHandler(S2C_STUN_APPLY);
			network->UnregisterPacketHandler(S2C_STUN_RELEASE);
			network->UnregisterPacketHandler(S2C_MOVE_ROLLBACK);
		}
		if (UOnlineSessionSubsystem* online = game_instance ? game_instance->GetSubsystem<UOnlineSessionSubsystem>() : nullptr)
		{
			online->on_changed_.RemoveAll(this);
		}
		if (APlayerCharacter* character = local_character_.Get())
		{
			character->on_action_started_.RemoveAll(this);
			character->on_shot_fired_.RemoveDynamic(this, &UMovementSyncSubsystem::HandleShotFired);
			character->on_weapon_selected_.RemoveAll(this);
			character->on_aim_toggled_.RemoveAll(this);
			if (UPlayerMovementComponent* movement = Cast<UPlayerMovementComponent>(character->GetCharacterMovement()))
			{
				movement->on_move_step_.RemoveAll(this);
			}
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
		BindLocalCharacter();
	}
	// 이번 게임 프레임에 계산한 번호의 패킷과 이동 패킷을 한 번에 보낸다.
	if (has_move_packet_)
	{
		has_move_packet_ = false;
		GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>()->SendPendingPackets();
	}
}

void UMovementSyncSubsystem::BindLocalCharacter()
{
	const APlayerController* controller = GetWorld()->GetFirstPlayerController();
	APlayerCharacter* character = controller ? Cast<APlayerCharacter>(controller->GetPawn()) : nullptr;
	if (character == local_character_.Get())
	{
		return;
	}
	if (APlayerCharacter* previous = local_character_.Get())
	{
		previous->on_action_started_.RemoveAll(this);
		previous->on_shot_fired_.RemoveDynamic(this, &UMovementSyncSubsystem::HandleShotFired);
		previous->on_weapon_selected_.RemoveAll(this);
		previous->on_aim_toggled_.RemoveAll(this);
		if (UPlayerMovementComponent* movement = Cast<UPlayerMovementComponent>(previous->GetCharacterMovement()))
		{
			movement->on_move_step_.RemoveAll(this);
		}
	}
	local_character_ = character;
	if (!character)
	{
		return;
	}
	character->on_action_started_.AddUObject(this, &UMovementSyncSubsystem::HandleActionStarted);
	character->on_shot_fired_.AddDynamic(this, &UMovementSyncSubsystem::HandleShotFired);
	// 장비와 조준은 플레이어가 바꾼 순간 알림을 받아 그 번호의 패킷으로 넣는다.
	character->on_weapon_selected_.AddUObject(this, &UMovementSyncSubsystem::HandleWeaponSelected);
	character->on_aim_toggled_.AddUObject(this, &UMovementSyncSubsystem::HandleAimToggled);
	if (UPlayerMovementComponent* movement = Cast<UPlayerMovementComponent>(character->GetCharacterMovement()))
	{
		movement->on_move_step_.AddUObject(this, &UMovementSyncSubsystem::HandleMoveStep);
	}
}

// 플레이어가 고른 무기를 이번 번호의 패킷으로 들고 있는다. 같은 번호의 동작보다 먼저 넣으므로 서버는 동작을 바꾼 무기로 판정한다.
void UMovementSyncSubsystem::HandleWeaponSelected(EWeaponType weapon)
{
	C2S_EQUIP_PACKET packet{};
	packet.weapon = static_cast<uint8>(weapon);
	AppendFramePacket(C2S_EQUIP, GetPacketBody(packet));
}

// 플레이어가 바꾼 조준 여부를 이번 번호의 패킷으로 들고 있는다. 서버는 이 번호의 이동부터 바뀐 조준으로 계산한다.
void UMovementSyncSubsystem::HandleAimToggled(bool is_aiming)
{
	C2S_AIM_PACKET packet{};
	packet.is_aiming = is_aiming;
	AppendFramePacket(C2S_AIM, GetPacketBody(packet));
}

// 끝난 번호 하나를 계산할 때마다 부른다. 그 번호까지의 들고 있던 패킷을 보낼 패킷 뒤에 옮기고, 그 뒤에 그 번호의 이동 패킷을 붙인다.
// 게임 프레임이 끝날 때 한 번에 보낸다.
void UMovementSyncSubsystem::HandleMoveStep(const FLocalMoveStep& step)
{
	UGameNetworkSubsystem* network = GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>();
	int32 moved_count = 0;
	while (moved_count < frame_packets_.Num() && frame_packets_[moved_count].frame_number <= step.frame_number)
	{
		network->AppendPendingPacket(frame_packets_[moved_count].type, frame_packets_[moved_count].payload);
		++moved_count;
	}
	frame_packets_.RemoveAt(0, moved_count);
	C2S_MOVE_PACKET packet{};
	packet.frame_number = step.frame_number;
	packet.move_state = step.move_state;
	packet.move_keys = step.move_keys;
	network->AppendPendingPacket(C2S_MOVE, GetPacketBody(packet));
	has_move_packet_ = true;
}

void UMovementSyncSubsystem::AppendFramePacket(uint8 type, TArray<uint8> payload)
{
	BindLocalCharacter();
	const APlayerCharacter* character = local_character_.Get();
	const UPlayerMovementComponent* movement = character ? Cast<UPlayerMovementComponent>(character->GetCharacterMovement()) : nullptr;
	if (!movement || payload.Num() < static_cast<int32>(sizeof(uint32)))
	{
		return;
	}
	const uint32 frame_number = movement->GetFrameNumber();
	FMemory::Memcpy(payload.GetData(), &frame_number, sizeof(frame_number));
	frame_packets_.Add({ frame_number, type, MoveTemp(payload) });
}

// 내 캐릭터 스폰을 받은 순간 번호 시계를 시작하고, 쌓인 스폰 알림을 바로 보낸다. 0번 이동 패킷은 0번이 끝나면 보낸다.
void UMovementSyncSubsystem::StartFrameClock()
{
	BindLocalCharacter();
	const APlayerCharacter* character = local_character_.Get();
	UPlayerMovementComponent* movement = character ? Cast<UPlayerMovementComponent>(character->GetCharacterMovement()) : nullptr;
	if (!movement)
	{
		return;
	}
	movement->StartFrameClock();
	frame_packets_.Reset();
	GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>()->SendPendingPackets();
}

// 발사 패킷은 조준 방향과 대상이 정해진 뒤 HandleShotFired가 넣는다. 재장전은 손에 든 무기를 서버가 알고 있으므로 번호만 보낸다.
void UMovementSyncSubsystem::HandleActionStarted(EPlayerAction action, EWeaponType weapon)
{
	if (action == EPlayerAction::FIRE)
	{
		return;
	}
	if (action == EPlayerAction::RELOAD)
	{
		AppendFramePacket(C2S_RELOAD, GetPacketBody(C2S_RELOAD_PACKET{}));
		return;
	}
	C2S_ACTION_PACKET packet{};
	packet.action = static_cast<uint8>(action);
	AppendFramePacket(C2S_ACTION, GetPacketBody(packet));
}

// 쏜 총의 퍼지기 전 조준 방향, 카메라 위치, 쏜 횟수와, 퍼진 탄이 맞은 다른 플레이어가 화면에서 향하던 틱 번호와 그 틱까지 간 정도를 보낸다.
// 다른 플레이어에 맞지 않았으면 대상은 0이고, 받은 가장 최근 틱 번호를 보낸다. 명중은 서버가 그 시점으로 되감아 판정한다.
void UMovementSyncSubsystem::HandleShotFired(const FShotResult& shot)
{
	C2S_FIRE_PACKET packet{};
	for (int32 axis = 0; axis < 3; ++axis)
	{
		packet.aim_direction[axis] = static_cast<float>(shot.aim_direction[axis]);
		packet.camera[axis] = static_cast<float>(shot.camera_location[axis]);
	}
	packet.shot_count = shot.shot_count;
	packet.target_tick = latest_tick_number_;
	packet.target_progress = 100;
	const AActor* hit_actor = shot.has_hit ? shot.hit.GetActor() : nullptr;
	for (const auto& [player_id, remote] : remote_players_)
	{
		const URemoteMovementComponent* remote_movement = hit_actor && remote.Get() == hit_actor ? remote->FindComponentByClass<URemoteMovementComponent>() : nullptr;
		if (!remote_movement)
		{
			continue;
		}
		packet.target_player_id = player_id;
		packet.target_tick = remote_movement->GetTickNumber();
		packet.target_progress = remote_movement->GetTickProgress();
		break;
	}
	AppendFramePacket(C2S_FIRE, GetPacketBody(packet));
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

// 서버가 그 플레이어의 이동을 인정하지 않고 서버 이동 상태로 롤백했다. 다른 플레이어면 그 캐릭터를 옮기는 시간 없이 바로 옮긴다.
// 내 것이면 위치·속도·공중 여부를 지금 이동 상태로 바로 바꾸고 확인을 보낸다. 몸 방향은 내 조작을 따른다.
// 아직 보내지 않은 패킷은 롤백 전 상태에서 만든 것이라 버린다. 서버도 확인 전에 온 패킷을 버리고, 확인 뒤 첫 이동 패킷에서 처리할 틱을 다시 정한다.
void UMovementSyncSubsystem::HandleMoveRollbackPacket(const TArray<uint8>& payload)
{
	if (payload.Num() != sizeof(S2C_MOVE_ROLLBACK_PACKET) - sizeof(PACKET_HEADER))
	{
		HandleInvalidPacket();
		return;
	}
	S2C_MOVE_ROLLBACK_PACKET packet{};
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	MarkTickNumber(packet.tick_number);
	if (packet.player_id != GetWorld()->GetGameInstance()->GetSubsystem<UOnlineSessionSubsystem>()->GetPlayerKey())
	{
		const APlayerCharacter* remote = FindRemotePlayer(packet.player_id);
		if (URemoteMovementComponent* remote_movement = remote ? remote->FindComponentByClass<URemoteMovementComponent>() : nullptr)
		{
			remote_movement->ApplyRollback(ToRemoteMoveState(packet), packet.tick_number);
		}
		return;
	}
	APlayerCharacter* character = local_character_.Get();
	if (UPlayerMovementComponent* movement = character ? Cast<UPlayerMovementComponent>(character->GetCharacterMovement()) : nullptr)
	{
		movement->SetStepState(FVector(packet.location[0], packet.location[1], packet.location[2]), FVector(packet.velocity[0], packet.velocity[1], packet.velocity[2]), packet.is_falling);
	}
	UGameNetworkSubsystem* network = GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>();
	frame_packets_.Reset();
	network->ClearPendingPackets();
	has_move_packet_ = false;
	SendPacketBody(network, C2S_MOVE_ROLLBACK_ACK, C2S_MOVE_ROLLBACK_ACK_PACKET{});
}

void UMovementSyncSubsystem::MarkTickNumber(uint32 tick_number)
{
	// 틱 번호는 32비트로 돌아가므로 차이로 본다.
	if (static_cast<int32>(tick_number - latest_tick_number_) > 0)
	{
		latest_tick_number_ = tick_number;
	}
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
	MarkTickNumber(packet.tick_number);
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
		remote_movement->ApplyState(state, packet.tick_number);
	}
}

// 서버가 그 플레이어를 멈춘 것으로 판단했다. 서버가 정한 위치로 옮기고 수평 속도를 0으로 둔다.
void UMovementSyncSubsystem::HandleMoveStopPacket(const TArray<uint8>& payload)
{
	if (payload.Num() != sizeof(S2C_MOVE_STOP_PACKET) - sizeof(PACKET_HEADER))
	{
		HandleInvalidPacket();
		return;
	}
	S2C_MOVE_STOP_PACKET packet{};
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	MarkTickNumber(packet.tick_number);
	const APlayerCharacter* remote = FindRemotePlayer(packet.player_id);
	if (URemoteMovementComponent* remote_movement = remote ? remote->FindComponentByClass<URemoteMovementComponent>() : nullptr)
	{
		remote_movement->ApplyMoveStop(FVector(packet.location[0], packet.location[1], packet.location[2]), packet.tick_number);
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
	// 내 것이면 서버가 조준 변경을 받지 않은 것이다. 서버 조준으로 맞춘다. 맞춘 조준은 플레이어가 바꾼 것이 아니라 보내지 않는다.
	if (packet.player_id == GetWorld()->GetGameInstance()->GetSubsystem<UOnlineSessionSubsystem>()->GetPlayerKey())
	{
		if (APlayerCharacter* character = local_character_.Get())
		{
			character->SetAiming(packet.is_aiming);
		}
		return;
	}
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
	// 내 것이면 서버가 장비 교체를 받지 않은 것이다. 서버 장비로 맞춘다. 맞춘 장비는 플레이어가 고른 것이 아니라 보내지 않는다.
	if (packet.player_id == GetWorld()->GetGameInstance()->GetSubsystem<UOnlineSessionSubsystem>()->GetPlayerKey())
	{
		if (APlayerCharacter* character = local_character_.Get())
		{
			character->ApplyServerWeapon(ToWeaponType(packet.weapon));
		}
		return;
	}
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
