#include "OnlineSessionSubsystem.h"

#include <array>
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UObjectGlobals.h"
#include "TimerManager.h"
#include "Containers/StringConv.h"
#include "Flow/ClientStateSubsystem.h"
#include "GameNetworkSubsystem.h"
#include "game_protocol.h"

#define LOCTEXT_NAMESPACE "OnlineSession"
constexpr uint8 PACKET_TYPES[] = {S2C_LOGIN, S2C_ERROR, S2C_ADD_PUBLIC_ROOM, S2C_ADD_PRIVATE_ROOM, S2C_ROOM_INFO, S2C_ADD_PLAYER, S2C_REMOVE_PLAYER, S2C_READY, S2C_UPDATE_HOST, S2C_SPAWN, S2C_MULTI_START, S2C_INFO};

template<typename T>
static TOptional<T> ReadPacket(const TArray<uint8>& payload)
{
	if (payload.Num() != sizeof(T) - sizeof(PACKET_HEADER)) return {};
	T packet{};
	if (!payload.IsEmpty()) FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	return packet;
}

template<typename T>
static bool SendPacket(UGameNetworkSubsystem* network, uint8 type, const T& packet)
{
	TArray<uint8> payload;
	payload.Append(reinterpret_cast<const uint8*>(&packet) + sizeof(PACKET_HEADER), sizeof(T) - sizeof(PACKET_HEADER));
	return network->SendPacket(type, payload);
}

template<size_t N>
static TOptional<std::array<char, N>> EncodeText(const FString& text, int32 minimum)
{
	FTCHARToUTF8 encoded(*text);
	if (encoded.Length() < minimum || encoded.Length() > N) return {};
	std::array<char, N> result{};
	FMemory::Memcpy(result.data(), encoded.Get(), encoded.Length());
	return result;
}

template<size_t N>
static FString DecodeText(const char (&text)[N])
{
	int32 length = 0;
	while (length < N && text[length] != 0) ++length;
	FUTF8ToTCHAR decoded(text, length);
	return FString(decoded.Length(), decoded.Get());
}

static FText ErrorMessage(int32 code)
{
	switch (code) {
	case LOGIN_FAILED: return LOCTEXT("LoginFailed", "아이디 또는 비밀번호가 일치하지 않습니다.");
	case DUPLICATE_LOGIN_ID: return LOCTEXT("Duplicate", "이미 로그인된 계정입니다.");
	case ROOM_NOT_FOUND: return LOCTEXT("MissingRoom", "방이 없어졌습니다. 목록을 새로고침해 주세요.");
	case ROOM_FULL: return LOCTEXT("Full", "방의 인원이 가득 찼습니다.");
	case ROOM_IN_GAME: return LOCTEXT("Playing", "이미 게임이 시작된 방입니다.");
	case NOT_FOUND_JOINABLE_ROOM: return LOCTEXT("NoMatch", "참가 가능한 공개 방이 없습니다.");
	case ROOM_INVALID_PASSWORD: return LOCTEXT("RoomPassword", "방 비밀번호가 일치하지 않습니다.");
	case ROOM_NOT_ENOUGH_PLAYERS: return LOCTEXT("TooFew", "게임 시작에는 최소 2명이 필요합니다.");
	case ROOM_NOT_ALL_READY: return LOCTEXT("NotReady", "모든 참가자가 준비해야 합니다.");
	case INVALID_REQUEST: return LOCTEXT("InvalidRequest", "현재 상태에서는 요청을 처리할 수 없습니다.");
	case PLAYER_NOT_FOUND: return LOCTEXT("MissingPlayer", "해당 참가자가 방에 없습니다.");
	default: return LOCTEXT("ServerError", "서버에서 요청을 처리하지 못했습니다. 잠시 후 다시 시도해 주세요.");
	}
}

void UOnlineSessionSubsystem::Initialize(FSubsystemCollectionBase& collection)
{
	Super::Initialize(collection);
	collection.InitializeDependency<UGameNetworkSubsystem>();
	auto* network = GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>();
	for (uint8 type : PACKET_TYPES) network->RegisterPacketHandler(type, FNetworkPacketHandler::CreateUObject(this, &UOnlineSessionSubsystem::HandlePacket, type));
	network->on_connection_state_changed_.AddDynamic(this, &UOnlineSessionSubsystem::HandleConnectionChanged);
	GetGameInstance()->GetSubsystem<UClientStateSubsystem>()->on_state_changed_.AddDynamic(this, &UOnlineSessionSubsystem::HandleClientStateChanged);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UOnlineSessionSubsystem::HandleMapLoaded);
}

void UOnlineSessionSubsystem::Deinitialize()
{
	GetGameInstance()->GetTimerManager().ClearTimer(refresh_timer_);
	FinishRequest();
	auto* network = GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>();
	for (uint8 type : PACKET_TYPES) network->UnregisterPacketHandler(type);
	network->on_connection_state_changed_.RemoveAll(this);
	GetGameInstance()->GetSubsystem<UClientStateSubsystem>()->on_state_changed_.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
	on_changed_.Clear();
	Super::Deinitialize();
}

bool UOnlineSessionSubsystem::CanRequest(EClientState state) const
{
	const auto* flow = GetGameInstance()->GetSubsystem<UClientStateSubsystem>();
	return !flow->IsLocalMode() && flow->GetState() == state && !IsPending() && (state == EClientState::LOGIN || IsLoggedIn());
}

void UOnlineSessionSubsystem::BeginRequest(OnlineRequest request)
{
	pending_request_ = request;
	GetGameInstance()->GetTimerManager().SetTimer(request_timer_, this, &UOnlineSessionSubsystem::HandleTimeout, 20.0f, false);
	ShowStatus(LOCTEXT("Pending", "서버 응답을 기다리는 중입니다."));
}

void UOnlineSessionSubsystem::FinishRequest()
{
	GetGameInstance()->GetTimerManager().ClearTimer(request_timer_);
	pending_request_ = OnlineRequest::NONE;
	pending_kick_key_ = 0;
	status_ = FText::GetEmpty();
}

void UOnlineSessionSubsystem::ShowStatus(const FText& message)
{
	status_ = message;
	on_changed_.Broadcast();
}

void UOnlineSessionSubsystem::HandleTimeout()
{
	// 늦은 성공 응답이 다음 요청에 섞이지 않도록 연결 자체를 종료한다.
	GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>()->Disconnect();
	ShowStatus(LOCTEXT("Timeout", "서버 응답 시간이 초과되었습니다. 다시 로그인해 주세요."));
}

void UOnlineSessionSubsystem::Login(const FString& user_id, const FString& password)
{
	if (!CanRequest(EClientState::LOGIN)) return;
	const auto id = EncodeText<MAX_PLAYER_ID_SIZE>(user_id, 1);
	const auto secret = EncodeText<MAX_PLAYER_PASSWORD_SIZE>(password, 1);
	if (!id || !secret) { ShowStatus(LOCTEXT("LoginLength", "아이디 또는 비밀번호의 길이를 확인해 주세요.")); return; }
	auto* network = GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>();
	if (network->GetConnectionState() != ENetworkConnectionState::CONNECTED && !network->Reconnect()) return;
	C2S_LOGIN_PACKET packet{};
	FMemory::Memcpy(packet.login_id, id->data(), sizeof(packet.login_id));
	FMemory::Memcpy(packet.login_password, secret->data(), sizeof(packet.login_password));
	if (SendPacket(network, C2S_LOGIN, packet)) BeginRequest(OnlineRequest::LOGIN);
	else ShowStatus(LOCTEXT("SendFailed", "요청을 전송하지 못했습니다."));
}

void UOnlineSessionSubsystem::RefreshRooms()
{
	if (!CanRequest(EClientState::LOBBY)) return;
	rooms_.Reset();
	if (!SendPacket(GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>(), C2S_REQUEST_ROOM_LIST, C2S_REQUEST_ROOM_LIST_PACKET{})) ShowStatus(LOCTEXT("SendFailed", "요청을 전송하지 못했습니다."));
	else ShowStatus(FText::GetEmpty());
}

void UOnlineSessionSubsystem::CreateRoom(const FString& room_name, bool is_private, const FString& password)
{
	if (!CanRequest(EClientState::LOBBY)) return;
	const auto name = EncodeText<MAX_ROOM_NAME_SIZE>(room_name, 4);
	const auto secret = EncodeText<MAX_ROOM_PASSWORD_SIZE>(password, is_private ? 4 : 0);
	if (!name || !secret) { ShowStatus(LOCTEXT("RoomLength", "방 이름 또는 비밀번호가 너무 짧거나 깁니다.")); return; }
	auto* network = GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>();
	bool sent = false;
	if (is_private) {
		C2S_ADD_PRIVATE_ROOM_PACKET packet{};
		packet.max_player_count = MAX_ROOM_PLAYER_COUNT;
		FMemory::Memcpy(packet.room_name, name->data(), sizeof(packet.room_name));
		FMemory::Memcpy(packet.room_password, secret->data(), sizeof(packet.room_password));
		sent = SendPacket(network, C2S_ADD_PRIVATE_ROOM, packet);
	} else {
		C2S_ADD_PUBLIC_ROOM_PACKET packet{};
		packet.max_player_count = MAX_ROOM_PLAYER_COUNT;
		FMemory::Memcpy(packet.room_name, name->data(), sizeof(packet.room_name));
		sent = SendPacket(network, C2S_ADD_PUBLIC_ROOM, packet);
	}
	if (sent) BeginRequest(OnlineRequest::ENTER_ROOM);
	else ShowStatus(LOCTEXT("SendFailed", "요청을 전송하지 못했습니다."));
}

void UOnlineSessionSubsystem::JoinRoom(int64 room_key, const FString& password)
{
	if (!CanRequest(EClientState::LOBBY)) return;
	const FLobbyRoomInfo* room = rooms_.FindByPredicate([room_key](const auto& item) { return item.room_key == room_key; });
	if (!room) { ShowStatus(ErrorMessage(ROOM_NOT_FOUND)); return; }
	auto* network = GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>();
	bool sent = false;
	if (room->is_private) {
		const auto secret = EncodeText<MAX_ROOM_PASSWORD_SIZE>(password, 1);
		if (!secret) { ShowStatus(ErrorMessage(ROOM_INVALID_PASSWORD)); return; }
		C2S_JOIN_PRIVATE_ROOM_PACKET packet{};
		packet.room_key = static_cast<RoomKey>(room_key);
		FMemory::Memcpy(packet.room_password, secret->data(), sizeof(packet.room_password));
		sent = SendPacket(network, C2S_JOIN_PRIVATE_ROOM, packet);
	} else {
		C2S_JOIN_PUBLIC_ROOM_PACKET packet{};
		packet.room_key = static_cast<RoomKey>(room_key);
		sent = SendPacket(network, C2S_JOIN_PUBLIC_ROOM, packet);
	}
	if (sent) BeginRequest(OnlineRequest::ENTER_ROOM);
	else ShowStatus(LOCTEXT("SendFailed", "요청을 전송하지 못했습니다."));
}

void UOnlineSessionSubsystem::QuickJoin()
{
	if (!CanRequest(EClientState::LOBBY)) return;
	C2S_FAST_MATCHING_PACKET packet{};
	packet.max_player_count = MAX_ROOM_PLAYER_COUNT;
	if (SendPacket(GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>(), C2S_FAST_MATCHING, packet)) BeginRequest(OnlineRequest::ENTER_ROOM);
	else ShowStatus(LOCTEXT("SendFailed", "요청을 전송하지 못했습니다."));
}

void UOnlineSessionSubsystem::LeaveRoom()
{
	if (!CanRequest(EClientState::ROOM)) return;
	if (SendPacket(GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>(), C2S_REMOVE_PLAYER, C2S_REMOVE_PLAYER_PACKET{})) BeginRequest(OnlineRequest::LEAVE);
	else ShowStatus(LOCTEXT("SendFailed", "요청을 전송하지 못했습니다."));
}

void UOnlineSessionSubsystem::SetReady(bool is_ready)
{
	if (!CanRequest(EClientState::ROOM) || host_key_ == player_key_) return;
	const auto* member = members_.FindByPredicate([this](const auto& item) { return item.player_key == player_key_; });
	if (!member || member->is_ready == is_ready) return;
	if (SendPacket(GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>(), C2S_READY, C2S_READY_PACKET{})) BeginRequest(OnlineRequest::READY);
	else ShowStatus(LOCTEXT("SendFailed", "요청을 전송하지 못했습니다."));
}

bool UOnlineSessionSubsystem::CanStart() const
{
	return host_key_ == player_key_ && members_.Num() >= 2 && !members_.ContainsByPredicate([](const auto& member) { return !member.is_host && !member.is_ready; });
}

void UOnlineSessionSubsystem::StartGame()
{
	if (!CanRequest(EClientState::ROOM) || !CanStart()) return;
	if (SendPacket(GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>(), C2S_START, C2S_START_PACKET{})) BeginRequest(OnlineRequest::START);
	else ShowStatus(LOCTEXT("SendFailed", "요청을 전송하지 못했습니다."));
}

void UOnlineSessionSubsystem::KickPlayer(int64 player_key)
{
	if (!CanRequest(EClientState::ROOM) || host_key_ != player_key_ || player_key == player_key_) return;
	if (!members_.ContainsByPredicate([player_key](const auto& item) { return item.player_key == player_key; })) return;
	C2S_KICK_PACKET packet{};
	packet.kick_player_id = static_cast<int32>(player_key);
	if (SendPacket(GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>(), C2S_KICK, packet)) {
		BeginRequest(OnlineRequest::KICK);
		pending_kick_key_ = player_key;
	} else ShowStatus(LOCTEXT("SendFailed", "요청을 전송하지 못했습니다."));
}

void UOnlineSessionSubsystem::EnterRoom(const FLobbyRoomInfo& room)
{
	FinishRequest();
	room_ = room;
	is_kicked_ = false;
	host_key_ = 0;
	members_.Reset();
	FRoomMemberInfo local;
	local.player_key = player_key_;
	local.nickname = nickname_;
	members_.Add(local);
	room_.player_count = 1;
	GetGameInstance()->GetSubsystem<UClientStateSubsystem>()->ChangeState(EClientState::ROOM);
}

void UOnlineSessionSubsystem::HandlePacket(const TArray<uint8>& payload, uint8 type)
{
	auto* flow = GetGameInstance()->GetSubsystem<UClientStateSubsystem>();
	if (flow->IsLocalMode()) return;
	const EClientState state = flow->GetState();
	bool valid = true;
	switch (type) {
	case S2C_LOGIN: {
		const auto packet = ReadPacket<S2C_LOGIN_PACKET>(payload);
		if (!packet || packet->player_id <= 0) { valid = false; break; }
		if (state != EClientState::LOGIN || pending_request_ != OnlineRequest::LOGIN) return;
		FinishRequest();
		player_key_ = packet->player_id;
		nickname_ = DecodeText(packet->nickname);
		flow->ChangeState(EClientState::LOBBY);
		break;
	}
	case S2C_ERROR: {
		const auto packet = ReadPacket<S2C_ERROR_PACKET>(payload);
		if (!packet) { valid = false; break; }
		FinishRequest();
		status_ = ErrorMessage(packet->error_code);
		break;
	}
	case S2C_ROOM_INFO: {
		const auto packet = ReadPacket<S2C_ROOM_INFO_PACKET>(payload);
		if (!packet) { valid = false; break; }
		if (state != EClientState::LOBBY) return;
		FLobbyRoomInfo room;
		room.room_key = static_cast<int64>(packet->room_key);
		room.room_name = DecodeText(packet->room_name);
		room.player_count = packet->current_player_count;
		room.max_player_count = packet->max_player_count;
		room.is_private = packet->is_private;
		room.is_playing = packet->is_play;
		if (auto* current = rooms_.FindByPredicate([&room](const auto& item) { return item.room_key == room.room_key; })) *current = room;
		else rooms_.Add(room);
		break;
	}
	case S2C_ADD_PUBLIC_ROOM: {
		const auto packet = ReadPacket<S2C_ADD_PUBLIC_ROOM_PACKET>(payload);
		if (!packet) { valid = false; break; }
		if (state != EClientState::LOBBY || pending_request_ != OnlineRequest::ENTER_ROOM) return;
		FLobbyRoomInfo room;
		room.room_key = static_cast<int64>(packet->room_key);
		room.room_name = DecodeText(packet->room_name);
		room.max_player_count = packet->max_player_count;
		EnterRoom(room);
		break;
	}
	case S2C_ADD_PRIVATE_ROOM: {
		const auto packet = ReadPacket<S2C_ADD_PRIVATE_ROOM_PACKET>(payload);
		if (!packet) { valid = false; break; }
		if (state != EClientState::LOBBY || pending_request_ != OnlineRequest::ENTER_ROOM) return;
		FLobbyRoomInfo room;
		room.room_key = static_cast<int64>(packet->room_key);
		room.room_name = DecodeText(packet->room_name);
		room.max_player_count = packet->max_player_count;
		room.is_private = true;
		EnterRoom(room);
		break;
	}
	case S2C_ADD_PLAYER: {
		const auto packet = ReadPacket<S2C_ADD_PLAYER_PACKET>(payload);
		if (!packet) { valid = false; break; }
		if (state != EClientState::ROOM) return;
		FRoomMemberInfo member;
		member.player_key = packet->player_id;
		member.nickname = DecodeText(packet->nickname);
		member.is_host = member.player_key == host_key_;
		if (auto* current = members_.FindByPredicate([&member](const auto& item) { return item.player_key == member.player_key; })) current->nickname = member.nickname;
		else if (members_.Num() < MAX_ROOM_PLAYER_COUNT) members_.Add(member);
		room_.player_count = members_.Num();
		break;
	}
	case S2C_REMOVE_PLAYER: {
		const auto packet = ReadPacket<S2C_REMOVE_PLAYER_PACKET>(payload);
		if (!packet) { valid = false; break; }
		if (state != EClientState::ROOM && state != EClientState::PLAYING && state != EClientState::LOADING) return;
		if (packet->player_id == player_key_) {
			FinishRequest();
			members_.Reset();
			room_ = {};
			host_key_ = 0;
			flow->ChangeState(EClientState::LOBBY);
			if (is_kicked_) status_ = LOCTEXT("Kicked", "방에서 퇴장되었습니다.");
			is_kicked_ = false;
			if (state != EClientState::ROOM) UGameplayStatics::OpenLevel(this, TEXT("/Game/Thats_Mine/Levels/Lvl_Lobby"));
		} else {
			members_.RemoveAll([&packet](const auto& item) { return item.player_key == packet->player_id; });
			room_.player_count = members_.Num();
			if (pending_request_ == OnlineRequest::KICK && packet->player_id == pending_kick_key_) FinishRequest();
		}
		break;
	}
	case S2C_READY: {
		const auto packet = ReadPacket<S2C_READY_PACKET>(payload);
		if (!packet) { valid = false; break; }
		if (state != EClientState::ROOM) return;
		if (auto* member = members_.FindByPredicate([&packet](const auto& item) { return item.player_key == packet->player_id; })) member->is_ready = packet->is_ready;
		if (packet->player_id == player_key_ && pending_request_ == OnlineRequest::READY) FinishRequest();
		break;
	}
	case S2C_UPDATE_HOST: {
		const auto packet = ReadPacket<S2C_UPDATE_HOST_PACKET>(payload);
		if (!packet) { valid = false; break; }
		if (state != EClientState::ROOM && state != EClientState::PLAYING) return;
		host_key_ = packet->new_host_id;
		if ((host_key_ == player_key_ && pending_request_ == OnlineRequest::READY) || (host_key_ != player_key_ && (pending_request_ == OnlineRequest::START || pending_request_ == OnlineRequest::KICK))) FinishRequest();
		for (auto& member : members_) member.is_host = member.player_key == host_key_;
		break;
	}
	case S2C_SPAWN: {
		// 항목 수만큼만 오는 가변 길이 패킷이다.
		constexpr int32 ENTRIES_OFFSET = static_cast<int32>(offsetof(S2C_SPAWN_PACKET, entries) - sizeof(PACKET_HEADER));
		if (payload.Num() < ENTRIES_OFFSET) { valid = false; break; }
		const int32 player_count = payload[0];
		if (player_count > MAX_ROOM_PLAYER_COUNT || payload.Num() != ENTRIES_OFFSET + player_count * static_cast<int32>(sizeof(S2C_SPAWN_ENTRY))) { valid = false; break; }
		if (state != EClientState::ROOM) return;
		spawn_transform_.Reset();
		for (int32 index = 0; index < player_count; ++index)
		{
			S2C_SPAWN_ENTRY entry;
			FMemory::Memcpy(&entry, payload.GetData() + ENTRIES_OFFSET + index * sizeof(S2C_SPAWN_ENTRY), sizeof(entry));
			if (entry.player_id != player_key_) continue;
			spawn_transform_ = FTransform(FRotator(0.0, entry.yaw, 0.0), FVector(entry.location[0], entry.location[1], entry.location[2]));
		}
		break;
	}
	case S2C_MULTI_START: {
		if (!ReadPacket<S2C_MULTI_START_PACKET>(payload)) { valid = false; break; }
		if (state != EClientState::ROOM) return;
		FinishRequest();
		room_.is_playing = true;
		flow->ChangeState(EClientState::LOADING);
		UGameplayStatics::OpenLevel(this, TEXT("/Game/Thats_Mine/Levels/Demo_Landmass"));
		break;
	}
	case S2C_INFO: {
		const auto packet = ReadPacket<S2C_INFO_PACKET>(payload);
		if (!packet) { valid = false; break; }
		if (packet->info_code == KICKED) { is_kicked_ = true; status_ = LOCTEXT("Kicked", "방에서 퇴장되었습니다."); }
		break;
	}
	default: return;
	}
	if (!valid) {
		GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>()->Disconnect();
		ShowStatus(LOCTEXT("InvalidPacket", "서버 응답 형식이 올바르지 않습니다."));
		return;
	}
	NotifyChanged();
}

void UOnlineSessionSubsystem::ResetSession()
{
	GetGameInstance()->GetTimerManager().ClearTimer(refresh_timer_);
	has_pending_refresh_ = false;
	is_kicked_ = false;
	FinishRequest();
	rooms_.Reset();
	members_.Reset();
	room_ = {};
	player_key_ = 0;
	host_key_ = 0;
	spawn_transform_.Reset();
	nickname_.Reset();
}

void UOnlineSessionSubsystem::ReturnToLogin(const FText& message)
{
	auto* flow = GetGameInstance()->GetSubsystem<UClientStateSubsystem>();
	const bool was_playing = flow->GetState() == EClientState::PLAYING || flow->GetState() == EClientState::LOADING;
	ResetSession();
	status_ = message;
	flow->ChangeState(EClientState::LOGIN);
	if (was_playing) UGameplayStatics::OpenLevel(this, TEXT("/Game/Thats_Mine/Levels/Lvl_Login"));
	on_changed_.Broadcast();
}

void UOnlineSessionSubsystem::HandleConnectionChanged(ENetworkConnectionState state, const FString& message)
{
	if (GetGameInstance()->GetSubsystem<UClientStateSubsystem>()->IsLocalMode()) { ResetSession(); return; }
	if (state == ENetworkConnectionState::CONNECTED) return;
	ReturnToLogin(LOCTEXT("Disconnected", "서버에 연결되지 않았습니다. 로그인 버튼으로 다시 연결할 수 있습니다."));
}

void UOnlineSessionSubsystem::HandleClientStateChanged(EClientState state)
{
	if (GetGameInstance()->GetSubsystem<UClientStateSubsystem>()->IsLocalMode()) ResetSession();
}

void UOnlineSessionSubsystem::HandleMapLoaded(UWorld* world)
{
	if (!world || world->GetGameInstance() != GetGameInstance()) return;
	auto* flow = GetGameInstance()->GetSubsystem<UClientStateSubsystem>();
	if (!flow->IsLocalMode() && IsLoggedIn() && flow->GetState() == EClientState::LOADING) flow->ChangeState(EClientState::PLAYING);
}

void UOnlineSessionSubsystem::NotifyChanged()
{
	if (has_pending_refresh_) return;
	has_pending_refresh_ = true;
	// 한 프레임에 받은 여러 방/참가자 패킷을 모아 위젯을 한 번 갱신한다.
	refresh_timer_ = GetGameInstance()->GetTimerManager().SetTimerForNextTick(this, &UOnlineSessionSubsystem::FlushChanges);
}

void UOnlineSessionSubsystem::FlushChanges()
{
	has_pending_refresh_ = false;
	on_changed_.Broadcast();
}

#undef LOCTEXT_NAMESPACE
