#include <algorithm>
#include <cmath>
#include <iterator>
#include <optional>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <random>
#include <utility>
#include "GameRoom.h"
#include "GameServer.h"
#include "char_buffer.h"
#include "SessionManager.h"
#include "packet_types.h"
#include "common_packets.h"
#include "room_packets.h"
#include "game_packets.h"
#include "spawn_points.h"
#include "platform_layout.h"
#include "spread_selection.h"
#include "hitbox_raycast.h"
#include "lobby_tasks.h"
#include "room_lifecycle_tasks.h"

GameRoom::~GameRoom()
{
}

// 서버가 방 배열을 만들 때 한 번 호출한다. 방 번호는 배열 위치이므로 바뀌지 않는다.
void GameRoom::AttachToServer(GameServer* server, int room_index)
{
	server_ = server;
	room_index_ = room_index;
}

// 빈 자리를 새 방 생성에 차지한다. 방 번호 뮤텍스 안에서 호출된다.
bool GameRoom::TryClaim()
{
	RoomState expected_state = RoomState::EMPTY;
	return room_state_.compare_exchange_strong(expected_state, RoomState::INITIALIZING);
}

// 차지한 자리에 새 방 정보를 기록하고, 마지막에 새 방 키를 공개한다.
void GameRoom::Initialize(const RoomInitData& data)
{
	memcpy(room_name_, data.room_name, sizeof(room_name_));
	memcpy(room_password_, data.room_password, sizeof(room_password_));
	is_private_.store(room_password_[0] != 0);
	host_id_ = -1;
	current_player_count_.store(0);
	room_key_.store(data.room_key, std::memory_order_release);
}

// 삭제 대기 중인 방을 초기화하기 위해 처리권을 가져온다. 이전 틱이 처리 중이면 실패하고, 호출한 쪽이 다음에 다시 시도한다.
bool GameRoom::TryBeginReset()
{
	RoomProcessState expected_state = RoomProcessState::COMPLETE;
	return processing_state_.compare_exchange_strong(expected_state, RoomProcessState::FREE);
}

// 방 자리를 비운다. 처리 상태가 FREE인 동안(처리 중인 게임 스레드가 없을 때)만 호출한다.
// 방 키를 먼저 0으로 바꿔, 방 정보를 읽던 쪽이 전후 키 비교로 재사용을 알아채게 한다.
void GameRoom::Reset()
{
	room_key_.store(0, std::memory_order_relaxed);
	std::atomic_thread_fence(std::memory_order_release);
	memset(room_name_, 0, sizeof(room_name_));
	memset(room_password_, 0, sizeof(room_password_));
	is_private_.store(false);
	for (auto& room_player : room_players_) room_player.ClearPlayer();
	host_id_ = -1;
	current_player_count_.store(0);
	pending_action_count_ = 0;
	room_state_.store(RoomState::EMPTY);
}

// 방 정보를 복사한다. 읽는 도중 방이 초기화·재사용됐으면 방 키가 달라지므로 false를 반환하고, 복사한 값은 버린다.
bool GameRoom::TryGetRoomInfoSnapshot(RoomInfoSnapshot& snapshot) const
{
	const RoomKey room_key = room_key_.load(std::memory_order_acquire);
	if (room_key == 0) return false;
	snapshot.room_key = room_key;
	snapshot.room_state = room_state_.load();
	snapshot.max_player_count = MAX_ROOM_PLAYER_COUNT;
	snapshot.current_player_count = static_cast<int>(current_player_count_.load());
	memcpy(snapshot.room_name, room_name_, sizeof(snapshot.room_name));
	snapshot.is_private = is_private_.load();
	std::atomic_thread_fence(std::memory_order_acquire);
	return room_key_.load(std::memory_order_relaxed) == room_key;
}

int GameRoom::GetCurrentPlayerCount() const
{
	return static_cast<int>(current_player_count_.load());
}

Session* GameRoom::FindSession(const Player& player) const
{
	if (!player.HasSession()) return nullptr;
	return server_->GetSessionManager().FindSession(player.GetSessionKey());
}

Player* GameRoom::FindPlayer(SessionKey session_key)
{
	auto& room_players = room_players_;
	for (auto& room_player : room_players) {
		if (room_player.MatchesSessionKey(session_key)) return &room_player;
	}
	return nullptr;
}

// 이 방이 담당 중인 플레이어인지 확인한다. 담당 상태가 ROOM이고 이 방 슬롯에 있으면 이 방 소속이다.
bool GameRoom::IsOwnedPlayer(const Session& session, SessionKey session_key)
{
	return session.GetModeState() == ModeState::ROOM && FindPlayer(session_key) != nullptr;
}

// 방이 담당 중인 세션을 로비로 넘긴다. 전환 대기로 먼저 바꾼 뒤 방에서 제거하고, 로비 통지는 마지막에 넣는다.
void GameRoom::LeaveToLobby(SessionKey session_key, RoomExitType exit_type)
{
	if (!FindPlayer(session_key)) return;
	auto* session = server_->GetSessionManager().FindSession(session_key);
	if (!session || !session->TryChangeMode(ModeState::ROOM, ModeState::LOBBY_PENDING)) return;
	CompletePlayerRemoval(session_key, exit_type);
	// 방을 떠나 로비 목록에 있어야 하는 상태가 되었으므로 목록을 맞춘다.
	server_->SyncLobbyList(session_key.session_index);
	server_->EnqueueLobbyTask(std::make_unique<LobbyTask>(LobbyTaskType::RETURN_TO_LOBBY, session_key));
}

bool GameRoom::AddHostSession(SessionKey session_key)
{
	auto& room_players = room_players_;
	if (!room_players[0].InitPlayer(session_key)) return false;
	current_player_count_.store(1);
	host_id_ = session_key.player_id;
	return true;
}

// 비밀번호는 남는 부분이 0으로 채워진 고정 길이 배열이므로 배열 전체를 비교한다.
int GameRoom::AddPlayer(Session& new_session, SessionKey session_key, const char* room_password)
{
	if (new_session.GetLifeState() != LifeState::ACTIVE || new_session.GetModeState() != ModeState::ROOM_PENDING) return ErrorCode::INVALID_REQUEST;
	if (memcmp(room_password_, room_password, sizeof(room_password_)) != 0) return ErrorCode::ROOM_INVALID_PASSWORD;
	const RoomState room_state = room_state_.load();
	if (room_state == RoomState::PLAY) return ErrorCode::ROOM_IN_GAME;
	if (room_state != RoomState::WAIT) return ErrorCode::ROOM_NOT_FOUND;
	auto& room_players = room_players_;
	if (FindPlayer(session_key)) return ErrorCode::INVALID_REQUEST;
	int added_slot = -1;
	for (int i = 0; i < room_players.size(); ++i) {
		if (room_players[i].HasSession()) continue;
		added_slot = i;
		break;
	}
	if (added_slot < 0) return ErrorCode::ROOM_FULL;

	// 방이 받는 쪽으로서 입장을 확정한다. 확정된 뒤부터 이 방이 세션 작업을 처리한다.
	if (!new_session.TryChangeMode(ModeState::ROOM_PENDING, ModeState::ROOM)) return ErrorCode::INVALID_REQUEST;
	room_players[added_slot].InitPlayer(session_key);
	++current_player_count_;

	if (!IsPrivate()) {
		S2C_ADD_PUBLIC_ROOM_PACKET public_p;
		public_p.header.size = static_cast<std::uint16_t>(sizeof(public_p));
		public_p.header.type = S2C_ADD_PUBLIC_ROOM;
		public_p.room_key = GetRoomKey();
		public_p.max_player_count = MAX_ROOM_PLAYER_COUNT;
		memcpy(public_p.room_name, room_name_, sizeof(public_p.room_name));
		new_session.SendPacket(reinterpret_cast<char*>(&public_p), public_p.header.size);
	}
	else {
		S2C_ADD_PRIVATE_ROOM_PACKET private_p;
		private_p.header.size = static_cast<std::uint16_t>(sizeof(private_p));
		private_p.header.type = S2C_ADD_PRIVATE_ROOM;
		private_p.room_key = GetRoomKey();
		private_p.max_player_count = MAX_ROOM_PLAYER_COUNT;
		memcpy(private_p.room_name, room_name_, sizeof(private_p.room_name));
		memcpy(private_p.room_password, room_password_, sizeof(private_p.room_password));
		new_session.SendPacket(reinterpret_cast<char*>(&private_p), private_p.header.size);
	}

	// 본인의 입장을 본인 제외 나머지에게(방 생성 시 본인은 방에 추가된다)
	for (int i = 0; i < room_players.size(); ++i) {
		if (i == added_slot) continue;
		auto& room_player = room_players[i];
		auto session = FindSession(room_player);
		if (!session) continue;
		S2C_ADD_PLAYER_PACKET add_p;
		add_p.header.size = static_cast<std::uint16_t>(sizeof(add_p));
		add_p.header.type = S2C_ADD_PLAYER;
		add_p.player_id = new_session.GetSessionKey().player_id;
		StringToCharBuf(new_session.GetNickname(), add_p.nickname, sizeof(add_p.nickname));
		session->SendPacket(reinterpret_cast<char*>(&add_p), add_p.header.size);
	}

	// 본인 제외 나머지 플레이어를 본인에게
	for (int i = 0; i < room_players.size(); ++i)	{
		if (i == added_slot) continue;
		auto& room_player = room_players[i];
		auto session = FindSession(room_player);
		if (!session) continue;
		S2C_ADD_PLAYER_PACKET add_p;
		add_p.header.size = static_cast<std::uint16_t>(sizeof(add_p));
		add_p.header.type = S2C_ADD_PLAYER;
		add_p.player_id = session->GetSessionKey().player_id;
		StringToCharBuf(session->GetNickname(), add_p.nickname, sizeof(add_p.nickname));
		new_session.SendPacket(reinterpret_cast<char*>(&add_p), add_p.header.size);
		S2C_READY_PACKET ready_p{};
		ready_p.header = {sizeof(ready_p), S2C_READY};
		ready_p.player_id = add_p.player_id;
		ready_p.is_ready = room_player.GetRoomPlayerState() == RoomPlayerState::READY;
		new_session.SendPacket(reinterpret_cast<const char*>(&ready_p), sizeof(ready_p));
	}

	// 새로 입장한 세션에게 방장이 누구인지
	S2C_UPDATE_HOST_PACKET host_p;
	host_p.header.size = static_cast<std::uint16_t>(sizeof(host_p));
	host_p.header.type = S2C_UPDATE_HOST;
	host_p.new_host_id = host_id_;
	new_session.SendPacket(reinterpret_cast<char*>(&host_p), host_p.header.size);
	return SUCCESS;
}

void GameRoom::HandlePacket(char* packet, Session& request_session)
{
	switch (reinterpret_cast<PACKET_HEADER*>(packet)->type) {
	case C2S_READY: {
		RoomTask task;
		task.task_type = RoomTaskType::READY;
		task.session_key = request_session.GetSessionKey();
		task.room_key = GetRoomKey();
		AddRoomTask(std::move(task));
		break;
	}
	case C2S_KICK: {
		C2S_KICK_PACKET* kick_p = reinterpret_cast<C2S_KICK_PACKET*>(packet);
		RoomTask task;
		task.task_type = RoomTaskType::KICK;
		task.session_key = request_session.GetSessionKey();
		task.target_player_id = kick_p->kick_player_id;
		task.room_key = GetRoomKey();
		AddRoomTask(std::move(task));
		break;
	}
	case C2S_REMOVE_PLAYER: {
		LeaveToLobby(request_session.GetSessionKey(), RoomExitType::LEAVE);
		break;
	}
	case C2S_START: {
		RoomTask task;
		task.task_type = RoomTaskType::START;
		task.session_key = request_session.GetSessionKey();
		task.room_key = GetRoomKey();
		AddRoomTask(std::move(task));
		break;
	}
	case C2S_MOVE:
	case C2S_MOVE_INPUT: {
		if (room_state_.load() != RoomState::PLAY) break;
		auto* room_player = FindPlayer(request_session.GetSessionKey());
		// 기절 중에는 이동·동작·명중을 받지 않는다. 되돌리기 확인을 기다리는 동안 받은 이동 상태도 버린다.
		if (!room_player || room_player->GetRoomPlayerState() != RoomPlayerState::PLAY || room_player->IsStunned(tick_time_ms_) || room_player->IsAwaitingMoveCorrection()) break;
		const MOVE_STATE& move_state = reinterpret_cast<C2S_MOVE_PACKET*>(packet)->move_state;
		if (!CheckMove(*room_player, move_state)) {
			RollbackMove(*room_player, request_session);
			break;
		}
		const bool is_input_change = reinterpret_cast<PACKET_HEADER*>(packet)->type == C2S_MOVE_INPUT;
		room_player->SetMoveState(move_state, tick_time_ms_, is_input_change);
		break;
	}
	case C2S_MOVE_CORRECTION_ACK: {
		if (room_state_.load() != RoomState::PLAY) break;
		auto* room_player = FindPlayer(request_session.GetSessionKey());
		if (room_player) room_player->CompleteMoveCorrection(reinterpret_cast<C2S_MOVE_CORRECTION_ACK_PACKET*>(packet)->correction_id);
		break;
	}
	case C2S_ACTION: {
		if (room_state_.load() != RoomState::PLAY || pending_action_count_ >= pending_actions_.size()) break;
		auto* room_player = FindPlayer(request_session.GetSessionKey());
		if (!room_player || room_player->GetRoomPlayerState() != RoomPlayerState::PLAY || room_player->IsStunned(tick_time_ms_)) break;
		const auto* request_p = reinterpret_cast<C2S_ACTION_PACKET*>(packet);
		auto& action_p = pending_actions_[pending_action_count_++];
		action_p.header = { sizeof(action_p), S2C_ACTION };
		action_p.player_id = request_session.GetSessionKey().player_id;
		action_p.server_time_ms = tick_time_ms_;
		action_p.action = request_p->action;
		action_p.weapon = request_p->weapon;
		room_player->GetPose().StartAction(request_p->action, request_p->weapon, tick_time_ms_);
		break;
	}
	case C2S_HIT: {
		if (room_state_.load() != RoomState::PLAY) break;
		auto* room_player = FindPlayer(request_session.GetSessionKey());
		if (!room_player || room_player->GetRoomPlayerState() != RoomPlayerState::PLAY || room_player->IsStunned(tick_time_ms_)) break;
		ProcessHit(*room_player, *reinterpret_cast<C2S_HIT_PACKET*>(packet));
		break;
	}
	case C2S_LOAD_COMPLETE: {
		if (room_state_.load() != RoomState::PLAY) break;
		auto* room_player = FindPlayer(request_session.GetSessionKey());
		if (room_player && room_player->GetRoomPlayerState() == RoomPlayerState::LOADING) room_player->SetRoomPlayerState(RoomPlayerState::LOADED);
		break;
	}
	default:
		break;
	}
}

// 방 키가 다르면 같은 자리에 새로 만들어진 다른 방이므로 받지 않는다.
bool GameRoom::AddRoomTask(RoomTask task)
{
	std::lock_guard<std::mutex> lock(room_mutex_);
	const RoomState state = room_state_.load();
	if (state != RoomState::WAIT && state != RoomState::PLAY) return false;
	if (task.room_key != GetRoomKey()) return false;
	room_tasks_.Enqueue(std::move(task));
	return true;
}

// 이 방이 담당하는 세션의 작업만 처리한다. 연결이 닫힌 세션은 방에서 제거한 뒤 담당을 놓는다.
void GameRoom::ProcessSessionTasks()
{
	auto& room_players = room_players_;
	for (auto& room_player : room_players) {
		if (!room_player.HasSession()) continue;
		const SessionKey session_key = room_player.GetSessionKey();
		auto* session = server_->GetSessionManager().FindSession(session_key);
		if (!session) continue;
		const SessionState state = session->GetState();
		if (state.mode != ModeState::ROOM) continue;

		const SessionDrainResult result = state.life == LifeState::ACTIVE ? server_->DrainSessionTasks(*session, this) : SessionDrainResult::CLOSED;
		if (result != SessionDrainResult::CLOSED) continue;
		CompletePlayerRemoval(session_key, RoomExitType::LEAVE);
		server_->GetSessionManager().ReleaseOwnership(*session);
	}
}

// 받는 쪽으로서 입장 요청을 확정한다. 성공하면 ROOM, 실패하면 돌려보내며 모든 분기가 결과로 끝난다.
// 빠른 매칭에서 온 요청이 실패하면 매칭 상태로 되돌리고, 매치메이커가 다음 주기에 다시 배정한다.
void GameRoom::ProcessJoinTask(const RoomTask& task)
{
	// 한 세션에 걸린 전환 요청은 하나뿐이므로, 전환 대기가 아니면 이 요청의 대상이 아니다.
	auto* session = server_->GetSessionManager().FindSession(task.session_key);
	if (!session || session->GetModeState() != ModeState::ROOM_PENDING) return;

	// 다른 방으로 재사용되기 전의 입장 요청이면 방을 찾지 못한 것으로 거절한다.
	const int result = task.room_key == GetRoomKey() ? AddPlayer(*session, task.session_key, task.room_password) : ErrorCode::ROOM_NOT_FOUND;
	if (result == SUCCESS) {
		// ROOM이 되어 로비 목록에 있으면 안 되므로 목록을 맞춘다.
		server_->SyncLobbyList(task.session_key.session_index);
		return;
	}

	if (task.is_matching) {
		session->TryChangeMode(ModeState::ROOM_PENDING, ModeState::MATCHING);
		return;
	}
	session->TryChangeMode(ModeState::ROOM_PENDING, ModeState::LOBBY_PENDING);
	auto lobby_task = std::make_unique<LobbyTask>(LobbyTaskType::RETURN_TO_LOBBY, task.session_key);
	lobby_task->result = result;
	server_->EnqueueLobbyTask(std::move(lobby_task));
}

void GameRoom::ProcessRoomTask(const RoomTask& task)
{
	if (task.task_type == RoomTaskType::JOIN) {
		ProcessJoinTask(task);
		return;
	}
	auto* session = server_->GetSessionManager().FindSession(task.session_key);
	if (!session || !IsOwnedPlayer(*session, task.session_key)) return;
	ProcessSpecificRoomTask(task);
}

void GameRoom::ProcessRoomTasks()
{
	const std::size_t task_count = room_tasks_.ClaimTaskCount();
	for (std::size_t i = 0; i < task_count; ++i) ProcessRoomTask(room_tasks_.Dequeue());
	TryPostRoomDelete();
}

void GameRoom::ProcessRoomTick(long long tick_time_ms)
{
	tick_time_ms_ = tick_time_ms;
	ProcessRoomTasks();
	ProcessSessionTasks();
	ProcessRoomTasks();
	if (room_state_.load() == RoomState::PLAY) ProcessGameTick(tick_time_ms);
}

// 생성이 끝난 방을 대기 상태로 열고, 처리 상태를 FREE에서 COMPLETE로 바꿔 게임 스레드가 처리하기 시작하게 한다.
void GameRoom::CompleteRoomInitialization()
{
	StoreRoomState(RoomState::WAIT);
	processing_state_.store(RoomProcessState::COMPLETE);
}

void GameRoom::BeginRoomDelete()
{
	std::lock_guard<std::mutex> lock(room_mutex_);
	StoreRoomState(RoomState::WAITING_DELETE);
}

void GameRoom::TryPostRoomDelete()
{
	{
		std::lock_guard<std::mutex> lock(room_mutex_);
		if (room_state_.load() != RoomState::WAITING_DELETE || room_tasks_.GetTaskCount() != 0) return;
		StoreRoomState(RoomState::DELETE_POST);
	}
	server_->EnqueueRoomLifecycleTask(std::make_unique<RoomLifecycleTask>(room_index_, GetRoomKey()));
}

void GameRoom::ClearGame()
{
	StoreRoomState(RoomState::WAIT);
	auto& room_players = room_players_;
	for (auto& room_player : room_players) {
		auto session = FindSession(room_player);
		if (!session) continue;
		room_player.ResetGameData();
	}
}

void GameRoom::StoreRoomState(RoomState new_state)
{
	room_state_.store(new_state);
}

bool GameRoom::TryChangeRoomState(RoomState expected, RoomState desired)
{
	return room_state_.compare_exchange_strong(expected, desired);
}


// 슬롯을 먼저 비운 뒤 남은 플레이어에게 퇴장을 알린다. 퇴장자는 로비 복귀 시 로비가 별도로 알린다.
void GameRoom::CompletePlayerRemoval(SessionKey session_key, RoomExitType exit_type)
{
	auto* room_player = FindPlayer(session_key);
	if (!room_player) return;
	const int player_id = room_player->GetSessionKey().player_id;
	if (current_player_count_.load() == 1) BeginRoomDelete();
	if (exit_type == RoomExitType::KICK) {
		auto* session = server_->GetSessionManager().FindSession(session_key);
		if (session) {
			S2C_INFO_PACKET info_p;
			info_p.header.size = static_cast<std::uint16_t>(sizeof(info_p));
			info_p.header.type = S2C_INFO;
			info_p.info_code = InfoCode::KICKED;
			session->SendPacket(reinterpret_cast<char*>(&info_p), info_p.header.size);
		}
	}
	room_player->ClearPlayer();
	--current_player_count_;

	S2C_REMOVE_PLAYER_PACKET p;
	p.header.size = static_cast<std::uint16_t>(sizeof(p));
	p.header.type = S2C_REMOVE_PLAYER;
	p.player_id = player_id;
	Broadcast(reinterpret_cast<char*>(&p));

	if (player_id == host_id_) FindNewHost(); // 여기서 찾기 및 못찾을 경우 삭제까지 같이 함
}

void GameRoom::SendCreateRoom(Session& session)
{
	if (!IsPrivate()) {
		S2C_ADD_PUBLIC_ROOM_PACKET public_p;
		public_p.header.size = static_cast<std::uint16_t>(sizeof(public_p));
		public_p.header.type = S2C_ADD_PUBLIC_ROOM;
		public_p.room_key = GetRoomKey();
		public_p.max_player_count = MAX_ROOM_PLAYER_COUNT;
		memcpy(public_p.room_name, room_name_, sizeof(public_p.room_name));
		session.SendPacket(reinterpret_cast<char*>(&public_p), public_p.header.size);
	}
	else {
		S2C_ADD_PRIVATE_ROOM_PACKET private_p;
		private_p.header.size = static_cast<std::uint16_t>(sizeof(private_p));
		private_p.header.type = S2C_ADD_PRIVATE_ROOM;
		private_p.room_key = GetRoomKey();
		private_p.max_player_count = MAX_ROOM_PLAYER_COUNT;
		memcpy(private_p.room_name, room_name_, sizeof(private_p.room_name));
		memcpy(private_p.room_password, room_password_, sizeof(private_p.room_password));
		session.SendPacket(reinterpret_cast<char*>(&private_p), private_p.header.size);
	}
	S2C_UPDATE_HOST_PACKET host_p{};
	host_p.header = {sizeof(host_p), S2C_UPDATE_HOST};
	host_p.new_host_id = host_id_;
	session.SendPacket(reinterpret_cast<const char*>(&host_p), sizeof(host_p));
}

void GameRoom::TogglePlayerReady(int player_id)
{
	if (host_id_ == player_id) return;
	auto& room_players = room_players_;
	bool is_ready = false;
	for (auto& room_player : room_players) {
		auto session = FindSession(room_player);
		if (!session) continue;
		if (session->GetSessionKey().player_id == player_id) { // 레디 상태
			if (room_player.GetRoomPlayerState() == RoomPlayerState::READY) {
				room_player.SetRoomPlayerState(RoomPlayerState::WAIT);
				is_ready = false;
			}
			else if (room_player.GetRoomPlayerState() == RoomPlayerState::WAIT) {
				room_player.SetRoomPlayerState(RoomPlayerState::READY);
				is_ready = true;
			}
			S2C_READY_PACKET p;
			p.header.size = static_cast<std::uint16_t>(sizeof(p));
			p.header.type = S2C_READY;
			p.player_id = player_id;
			p.is_ready = is_ready;

			Broadcast(reinterpret_cast<char*>(&p));
			break;
		}
	}
}

void GameRoom::KickPlayer(int requester_id, int kick_player_id)
{
	if (requester_id != host_id_) return;
	if (kick_player_id == host_id_) return;
	auto& room_players = room_players_;

	for (auto& room_player : room_players) {
		if (!room_player.HasSession() || room_player.GetSessionKey().player_id != kick_player_id) continue;
		LeaveToLobby(room_player.GetSessionKey(), RoomExitType::KICK);
		return;
	}
}

void GameRoom::StartGame(int requester_id)
{
	if (requester_id != host_id_) return;
	if (room_state_.load() == RoomState::PLAY) return;
	auto& room_players = room_players_;

	int ready_player_count = 0;
	int result = -1;
	for (auto& room_player : room_players) {
		auto session = FindSession(room_player);
		if (!session) continue; // 사용 중이지 않은 인덱스는 건너뜀
		if (session->GetSessionKey().player_id == host_id_) ++ready_player_count;

		else if (room_player.GetRoomPlayerState() == RoomPlayerState::READY) ++ready_player_count;
	}

	int current_player_count = GetCurrentPlayerCount();
	if (ready_player_count == current_player_count) {
		if (current_player_count == 1) {
			result = ErrorCode::ROOM_NOT_ENOUGH_PLAYERS;
		}
		else {
			result = SUCCESS;
		}
	}
	else result = ErrorCode::ROOM_NOT_ALL_READY;

	if (result != SUCCESS) {
		S2C_ERROR_PACKET error_p;
		error_p.header.size = static_cast<std::uint16_t>(sizeof(error_p));
		error_p.header.type = S2C_ERROR;
		error_p.error_code = result;
		int host_index = FindHostIndex(host_id_);
		if (host_index >= 0) {
			auto host_session = FindSession(room_players[host_index]);
			if (host_session) host_session->SendPacket(reinterpret_cast<char*>(&error_p), error_p.header.size);
		}
		return;
	}

	if(!TryChangeRoomState(RoomState::WAIT, RoomState::PLAY)) return; // 상태 전환에 성공한 요청만 게임을 시작한다.

	S2C_MULTI_START_PACKET start_p;
	start_p.header.size = static_cast<std::uint16_t>(sizeof(start_p));
	start_p.header.type = S2C_MULTI_START;
	Broadcast(reinterpret_cast<char*>(&start_p));
	// 시작 조건을 충족한 플레이어를 로딩 상태로 전환하고, 로딩 완료를 기다린다.
	for (auto& room_player : room_players) {
		if (FindSession(room_player)) room_player.SetRoomPlayerState(RoomPlayerState::LOADING);
	}
	loading_deadline_ms_ = tick_time_ms_ + LOADING_TIMEOUT_MS;
	game_start_ms_ = 0;
	is_game_started_ = false;
	last_pose_tick_ms_ = 0;
	// 이번 게임의 발판 배치를 정해 둔다.
	platform_layout_ = GeneratePlatformLayout(std::random_device{}());
	platform_layout_p_ = {};
	platform_layout_p_.header = { static_cast<std::uint16_t>(sizeof(platform_layout_p_)), S2C_PLATFORM_LAYOUT };
	platform_layout_p_.platform_count = static_cast<std::uint16_t>(PLATFORM_COORDINATE_COUNT);
	for (int index = 0; index < PLATFORM_COORDINATE_COUNT; ++index) {
		if (platform_layout_[index]) platform_layout_p_.spawned_bits[index / 8] |= static_cast<std::uint8_t>(1 << (index % 8));
	}
	// 이번 게임의 탑 자리를 후보 중에서 고르게 퍼뜨려 고른다. 보물은 놓인 탑을 피한다.
	const std::vector<DirectX::XMFLOAT3>& tower_candidates = server_->GetTowerCandidateData().GetCandidates();
	const std::vector<std::size_t> tower_indices = SelectSpreadIndices(tower_candidates, TOWER_COUNT, TOWER_MIN_DISTANCE);
	tower_layout_p_ = {};
	for (const std::size_t index : tower_indices) {
		const DirectX::XMFLOAT3& location = tower_candidates[index];
		float* entry = tower_layout_p_.locations[tower_layout_p_.tower_count++];
		entry[0] = location.x;
		entry[1] = location.y;
		entry[2] = location.z;
	}
	tower_layout_p_.header = { static_cast<std::uint16_t>(offsetof(S2C_TOWER_LAYOUT_PACKET, locations) + tower_layout_p_.tower_count * sizeof(tower_layout_p_.locations[0])), S2C_TOWER_LAYOUT };
	placed_objects_ = server_->GetMapData().GetCollisionObjects().CollectPlacedObjects(platform_layout_, tower_indices);
	// 이번 게임의 상자 자리를 정해 둔다. 상자 번호는 배치 패킷의 항목 순서다.
	box_placements_ = server_->GetBoxCandidateData().SelectPlacements(std::random_device{}());
	box_placements_.resize(std::min<std::size_t>(box_placements_.size(), MAX_BOX_COUNT));
	box_layout_p_ = {};
	box_layout_p_.box_count = static_cast<std::uint8_t>(box_placements_.size());
	for (std::size_t index = 0; index < box_placements_.size(); ++index) {
		const BoxPlacement& placement = box_placements_[index];
		box_layout_p_.entries[index] = { static_cast<std::uint8_t>(placement.kind), { placement.location.x, placement.location.y, placement.location.z }, { placement.rotation.x, placement.rotation.y, placement.rotation.z } };
	}
	box_layout_p_.header = { static_cast<std::uint16_t>(offsetof(S2C_BOX_LAYOUT_PACKET, entries) + box_layout_p_.box_count * sizeof(BOX_ENTRY)), S2C_BOX_LAYOUT };
	PlaceTreasure();
}

// 최종 보물을 맵 경계에서 TREASURE_EDGE_MARGIN 안쪽의 무작위 위치에 놓는다. 보물을 놓지 않는 영역(건물·동양 유적 뒤 절벽·시작 돔) 밖이고, 보물 둘레 사각형 안의 땅이 모두 있고,
// 보물을 감싼 구가 배치물(건물·나무·바위·발판·탑)과 겹치지 않는 곳이 나올 때까지 다시 뽑는다.
// 바닥은 그 사각형 안 가장 낮은 땅에 맞춰 뜨지 않게 한다.
void GameRoom::PlaceTreasure()
{
	const LandscapeData& landscape = server_->GetMapData().GetLandscape();
	const CollisionObjectData& collision_objects = server_->GetMapData().GetCollisionObjects();
	const MapRegionData& regions = server_->GetMapRegionData();
	std::mt19937 random(std::random_device{}());
	std::uniform_real_distribution<float> random_x(MAP_MIN[0] + TREASURE_EDGE_MARGIN, MAP_MAX[0] - TREASURE_EDGE_MARGIN);
	std::uniform_real_distribution<float> random_y(MAP_MIN[1] + TREASURE_EDGE_MARGIN, MAP_MAX[1] - TREASURE_EDGE_MARGIN);
	std::uniform_real_distribution<float> random_yaw(0.0f, 360.0f);
	treasure_location_ = {};
	treasure_yaw_ = 0.0f;
	for (int attempt = 0; attempt < TREASURE_PLACE_ATTEMPTS; ++attempt) {
		const float x = random_x(random);
		const float y = random_y(random);
		if (regions.IsTreasureExcluded(x, y)) continue;
		std::optional<float> ground = landscape.GetGroundHeight(x, y);
		for (int corner = 0; corner < 4 && ground; ++corner) {
			const std::optional<float> corner_ground = landscape.GetGroundHeight(x + ((corner & 1) ? TREASURE_RADIUS : -TREASURE_RADIUS), y + ((corner & 2) ? TREASURE_RADIUS : -TREASURE_RADIUS));
			ground = corner_ground ? std::optional<float>(std::min(*ground, *corner_ground)) : std::nullopt;
		}
		if (!ground) continue;
		if (collision_objects.IsCapsuleOverlappingObjects({ x, y, *ground + TREASURE_HALF_HEIGHT }, TREASURE_RADIUS, TREASURE_HALF_HEIGHT, TERRAIN_BLOCKS_PAWN, placed_objects_)) continue;
		treasure_location_ = { x, y, *ground };
		treasure_yaw_ = random_yaw(random);
		return;
	}
	std::cerr << "Treasure place fail.. " << std::endl;
}

void GameRoom::ProcessSpecificRoomTask(const RoomTask& task)
{
	const int requester_id = task.session_key.player_id;
	switch (task.task_type) {
	case RoomTaskType::READY:
		TogglePlayerReady(requester_id);
		break;
	case RoomTaskType::KICK:
		KickPlayer(requester_id, task.target_player_id);
		break;
	case RoomTaskType::START:
		StartGame(requester_id);
		break;
	default:
		break;
	}
}

// 중계할 이동 상태와 이번 틱에 받은 동작을 모아 받는 사람마다 한 번에 보낸다. 보낸 사람 것도 포함되며 클라이언트가 거른다.
void GameRoom::ProcessGameTick(long long tick_time_ms)
{
	ProcessLoadedPlayers();
	ProcessGameStart();
	// 플레이 중인 플레이어의 판정용 자세를 지난 틱 이후 시간만큼 진행한다.
	const float pose_delta_seconds = last_pose_tick_ms_ != 0 ? (tick_time_ms - last_pose_tick_ms_) / 1000.0f : 0.0f;
	last_pose_tick_ms_ = tick_time_ms;
	for (auto& room_player : room_players_) {
		if (!room_player.HasSession() || room_player.GetRoomPlayerState() != RoomPlayerState::PLAY || !room_player.HasMoveState()) continue;
		room_player.GetPose().Update(room_player.GetMoveState(), pose_delta_seconds, tick_time_ms);
	}
	char send_buffer[BUF_SIZE];
	int send_data_size = 0;
	const auto flush = [&]() {
		if (send_data_size == 0) return;
		for (auto& room_player : room_players_) {
			auto session = FindSession(room_player);
			if (!session || room_player.GetRoomPlayerState() != RoomPlayerState::PLAY) continue;
			session->SendPacket(send_buffer, send_data_size);
		}
		send_data_size = 0;
	};
	const auto append = [&](const void* packet, int packet_size) {
		if (send_data_size + packet_size > BUF_SIZE) flush();
		memcpy(send_buffer + send_data_size, packet, packet_size);
		send_data_size += packet_size;
	};

	S2C_MOVE_PACKET move_p;
	move_p.player_count = 0;
	for (auto& room_player : room_players_) {
		if (!room_player.HasSession() || !room_player.CanRelayMoveState(tick_time_ms)) continue;
		auto& entry = move_p.entries[move_p.player_count++];
		entry.player_id = room_player.GetSessionKey().player_id;
		entry.server_time_ms = room_player.GetMoveTimeMs();
		entry.move_state = room_player.GetMoveState();
		room_player.MarkMoveStateRelayed(tick_time_ms);
	}
	if (move_p.player_count > 0) {
		const int move_size = static_cast<int>(offsetof(S2C_MOVE_PACKET, entries) + move_p.player_count * sizeof(S2C_MOVE_ENTRY));
		move_p.header = { static_cast<std::uint16_t>(move_size), S2C_MOVE };
		append(&move_p, move_size);
	}
	for (std::size_t i = 0; i < pending_action_count_; ++i) append(&pending_actions_[i], sizeof(pending_actions_[i]));
	pending_action_count_ = 0;
	flush();
}

// 로딩을 마친 플레이어를 마치는 대로 스폰하고 맵을 보여 준다.
// 이미 시작 대기 중이면 남은 시간을, 게임이 시작됐으면 게임 시작을 이어서 보낸다.
void GameRoom::ProcessLoadedPlayers()
{
	bool has_loaded_player = false;
	for (auto& room_player : room_players_) {
		if (!FindSession(room_player)) continue;
		has_loaded_player |= room_player.GetRoomPlayerState() == RoomPlayerState::LOADED;
	}
	if (!has_loaded_player) return;

	// 시작 좌표는 방 슬롯 번호로 정한다. 이번에 합류하는 플레이어만 담은 것과 이미 플레이 중인 플레이어까지 담은 것을 만든다.
	S2C_SPAWN_PACKET joined_p;
	S2C_SPAWN_PACKET all_p;
	joined_p.player_count = 0;
	all_p.player_count = 0;
	for (int slot = 0; slot < MAX_ROOM_PLAYER_COUNT; ++slot) {
		auto& room_player = room_players_[slot];
		if (!FindSession(room_player)) continue;
		const RoomPlayerState state = room_player.GetRoomPlayerState();
		if (state != RoomPlayerState::LOADED && state != RoomPlayerState::PLAY) continue;
		const SpawnPoint& point = START_DOME_SPAWN_POINTS[slot];
		const S2C_SPAWN_ENTRY entry{ room_player.GetSessionKey().player_id, { point.x, point.y, point.z }, point.yaw };
		all_p.entries[all_p.player_count++] = entry;
		if (state == RoomPlayerState::LOADED) joined_p.entries[joined_p.player_count++] = entry;
	}
	joined_p.header = { static_cast<std::uint16_t>(offsetof(S2C_SPAWN_PACKET, entries) + joined_p.player_count * sizeof(S2C_SPAWN_ENTRY)), S2C_SPAWN };
	all_p.header = { static_cast<std::uint16_t>(offsetof(S2C_SPAWN_PACKET, entries) + all_p.player_count * sizeof(S2C_SPAWN_ENTRY)), S2C_SPAWN };
	S2C_PLAY_START_PACKET start_p;
	start_p.header = { static_cast<std::uint16_t>(sizeof(start_p)), S2C_PLAY_START };
	S2C_GAME_READY_PACKET ready_p;
	ready_p.header = { static_cast<std::uint16_t>(sizeof(ready_p)), S2C_GAME_READY };
	ready_p.remaining_ms = static_cast<std::uint32_t>(std::max(0LL, game_start_ms_ - tick_time_ms_));
	S2C_GAME_START_PACKET game_start_p;
	game_start_p.header = { static_cast<std::uint16_t>(sizeof(game_start_p)), S2C_GAME_START };

	// 합류하는 플레이어는 발판·탑·상자 배치, 전원의 스폰, 맵 표시를, 이미 플레이 중인 플레이어는 합류하는 플레이어의 스폰만 받는다.
	for (auto& room_player : room_players_) {
		auto session = FindSession(room_player);
		if (!session) continue;
		if (room_player.GetRoomPlayerState() == RoomPlayerState::LOADED) {
			room_player.GetPose().Reset(server_->GetHitboxData());
			session->SendPacket(reinterpret_cast<char*>(&platform_layout_p_), platform_layout_p_.header.size);
			session->SendPacket(reinterpret_cast<char*>(&tower_layout_p_), tower_layout_p_.header.size);
			session->SendPacket(reinterpret_cast<char*>(&box_layout_p_), box_layout_p_.header.size);
			session->SendPacket(reinterpret_cast<char*>(&all_p), all_p.header.size);
			session->SendPacket(reinterpret_cast<char*>(&start_p), start_p.header.size);
			if (is_game_started_) session->SendPacket(reinterpret_cast<char*>(&game_start_p), game_start_p.header.size);
			else if (game_start_ms_ != 0) session->SendPacket(reinterpret_cast<char*>(&ready_p), ready_p.header.size);
			room_player.SetRoomPlayerState(RoomPlayerState::PLAY);
		}
		else if (room_player.GetRoomPlayerState() == RoomPlayerState::PLAY) {
			session->SendPacket(reinterpret_cast<char*>(&joined_p), joined_p.header.size);
		}
	}
}

// 클라이언트가 보낸 명중을 검증하고, 인정하면 쏜 사람과 맞은 사람에게만 알린 뒤 맞은 사람의 기절 게이지를 올린다. 거부하면 아무것도 보내지 않는다.
void GameRoom::ProcessHit(const Player& shooter, const C2S_HIT_PACKET& request_p)
{
	Player* target = nullptr;
	for (auto& room_player : room_players_) {
		if (room_player.HasSession() && room_player.GetSessionKey().player_id == request_p.target_player_id) target = &room_player;
	}
	if (!target || target == &shooter || target->GetRoomPlayerState() != RoomPlayerState::PLAY || !CheckHit(shooter, *target, request_p)) return;

	S2C_HIT_PACKET hit_p;
	hit_p.header = { static_cast<std::uint16_t>(sizeof(hit_p)), S2C_HIT };
	hit_p.shooter_player_id = shooter.GetSessionKey().player_id;
	hit_p.target_player_id = request_p.target_player_id;
	hit_p.part = request_p.part;
	for (const Player* receiver : { &shooter, static_cast<const Player*>(target) }) {
		if (auto session = FindSession(*receiver)) session->SendPacket(reinterpret_cast<char*>(&hit_p), hit_p.header.size);
	}
	AddStunGauge(*target, request_p);
}

// 무기별 증가량에 맞은 부위 배율을 곱해 게이지를 올린다. 기절 중이면 게이지는 그대로다.
// 최대치에 닿으면 기절을 방 전체에 알리고 이동을 멈춘 상태로 중계한다. 아니면 바뀐 값을 맞은 사람에게만 보낸다.
void GameRoom::AddStunGauge(Player& target, const C2S_HIT_PACKET& request_p)
{
	if (request_p.weapon >= std::size(STUN_GAUGE_WEAPON_GAINS) || request_p.part >= std::size(STUN_GAUGE_PART_MULTIPLIERS) || target.IsStunned(tick_time_ms_)) return;
	if (target.AddStunGauge(STUN_GAUGE_WEAPON_GAINS[request_p.weapon] * STUN_GAUGE_PART_MULTIPLIERS[request_p.part], tick_time_ms_)) {
		target.StopMovement();
		S2C_STUN_PACKET stun_p;
		stun_p.header = { static_cast<std::uint16_t>(sizeof(stun_p)), S2C_STUN };
		stun_p.player_id = request_p.target_player_id;
		stun_p.duration_ms = STUN_DURATION_MS;
		for (auto& room_player : room_players_) {
			auto session = FindSession(room_player);
			if (session && room_player.GetRoomPlayerState() == RoomPlayerState::PLAY) session->SendPacket(reinterpret_cast<char*>(&stun_p), stun_p.header.size);
		}
		return;
	}
	S2C_STUN_GAUGE_PACKET gauge_p;
	gauge_p.header = { static_cast<std::uint16_t>(sizeof(gauge_p)), S2C_STUN_GAUGE };
	gauge_p.gauge = target.GetStunGauge(tick_time_ms_);
	if (auto session = FindSession(target)) session->SendPacket(reinterpret_cast<char*>(&gauge_p), gauge_p.header.size);
}

// 지연 보정이 없으므로 클라이언트 화면과 서버 위치 차이를 여유로 허용한다.
// 총인지, 사거리 안인지, 시작점이 쏜 사람 근처인지 본 뒤, 맞은 사람의 서버 히트박스를 속도에 비례한 여유만큼 키워 광선이 지나가는지 검사한다.
// 마지막으로 발사 지점부터 히트박스에 닿은 곳까지 랜드스케이프나 총알을 막는 배치물(건물·나무·바위)이 가로막지 않는지 본다.
bool GameRoom::CheckHit(const Player& shooter, const Player& target, const C2S_HIT_PACKET& request_p) const
{
	const HitboxData& data = server_->GetHitboxData();
	const WeaponInfo* weapon = data.GetWeaponInfo(request_p.weapon);
	if (!data.IsLoaded() || !weapon || !weapon->is_hitscan || request_p.part >= static_cast<std::uint8_t>(HitboxPart::COUNT)) return false;
	if (!shooter.HasMoveState() || !target.HasMoveState()) return false;
	if (!std::isfinite(request_p.distance) || request_p.distance < 0.0f || request_p.distance > weapon->hitscan_distance) return false;

	const DirectX::XMVECTOR direction = DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(request_p.direction));
	const float direction_length = DirectX::XMVectorGetX(DirectX::XMVector3Length(direction));
	if (!std::isfinite(direction_length) || direction_length < 0.5f) return false;
	DirectX::XMFLOAT3 unit_direction;
	DirectX::XMStoreFloat3(&unit_direction, DirectX::XMVectorScale(direction, 1.0f / direction_length));
	const DirectX::XMFLOAT3 origin(request_p.origin[0], request_p.origin[1], request_p.origin[2]);

	const MOVE_STATE& shooter_state = shooter.GetMoveState();
	const float shooter_speed = std::hypot(shooter_state.velocity[0], shooter_state.velocity[1], shooter_state.velocity[2]);
	const float origin_offset = std::hypot(origin.x - shooter_state.location[0], origin.y - shooter_state.location[1], origin.z - shooter_state.location[2]);
	if (!std::isfinite(origin_offset) || origin_offset > HIT_ORIGIN_MAX_OFFSET + shooter_speed * HIT_LATENCY_ALLOWANCE_SECONDS) return false;

	const MOVE_STATE& target_state = target.GetMoveState();
	const float target_speed = std::hypot(target_state.velocity[0], target_state.velocity[1], target_state.velocity[2]);
	const float margin = HIT_BASE_MARGIN + target_speed * HIT_LATENCY_ALLOWANCE_SECONDS;
	const float max_distance = request_p.distance + margin;
	const DirectX::XMFLOAT3 location(target_state.location[0], target_state.location[1], target_state.location[2]);
	if (!IsRayNearCharacter(origin, unit_direction, max_distance, location, target_state.yaw, data.GetBroadphaseCapsule(), margin)) return false;
	const HitboxRayHit hit = RaycastHitboxes(origin, unit_direction, max_distance, target.GetPose().BuildWorldHitboxes(target_state, tick_time_ms_), margin);
	if (!hit.has_hit) return false;
	if (hit.distance <= HIT_OCCLUSION_MARGIN * 2.0f) return true;
	const DirectX::XMVECTOR origin_v = DirectX::XMLoadFloat3(&origin);
	const DirectX::XMVECTOR direction_v = DirectX::XMLoadFloat3(&unit_direction);
	DirectX::XMFLOAT3 segment_start;
	DirectX::XMFLOAT3 segment_end;
	DirectX::XMStoreFloat3(&segment_start, DirectX::XMVectorAdd(origin_v, DirectX::XMVectorScale(direction_v, HIT_OCCLUSION_MARGIN)));
	DirectX::XMStoreFloat3(&segment_end, DirectX::XMVectorAdd(origin_v, DirectX::XMVectorScale(direction_v, hit.distance - HIT_OCCLUSION_MARGIN)));
	const LandscapeData& landscape = server_->GetMapData().GetLandscape();
	const CollisionObjectData& collision_objects = server_->GetMapData().GetCollisionObjects();
	return !landscape.IsSegmentBlockedByLandscape(segment_start, segment_end) && !collision_objects.IsSegmentBlockedByObjects(segment_start, segment_end, TERRAIN_BLOCKS_VISIBILITY, placed_objects_);
}

// 이전에 인정한 위치와 새 위치를 비교한다. 발이 랜드스케이프보다 허용치 넘게 아래면 땅속이고,
// 두 위치의 캡슐 중심을 이은 선분이 캐릭터 이동을 막는 배치물(건물·나무·바위·경계 볼륨·발판)을 지나면 관통이다.
// 새 위치의 캡슐이 이동을 막는 배치물에 허용치 넘게 파고들어도 인정하지 않는다. 게임 시작 전에는 시작 돔 밖으로 나간 위치도 인정하지 않는다. 첫 위치는 비교할 이전 위치가 없어 인정한다.
bool GameRoom::CheckMove(const Player& player, const MOVE_STATE& move_state) const
{
	const float* next = move_state.location;
	if (!std::isfinite(next[0]) || !std::isfinite(next[1]) || !std::isfinite(next[2])) return false;
	const LandscapeData& landscape = server_->GetMapData().GetLandscape();
	const CollisionObjectData& collision_objects = server_->GetMapData().GetCollisionObjects();
	if (!player.HasMoveState()) return true;
	const std::optional<float> ground = landscape.GetGroundHeight(next[0], next[1]);
	if (ground && next[2] - CHARACTER_CAPSULE_HALF_HEIGHT < *ground - MOVE_TOLERANCE) return false;
	// 캡슐을 허용치만큼 줄여도 겹치면 허용치보다 깊이 파고든 것이다.
	if (collision_objects.IsCapsuleOverlappingObjects({ next[0], next[1], next[2] }, CHARACTER_CAPSULE_RADIUS - MOVE_TOLERANCE, CHARACTER_CAPSULE_HALF_HEIGHT - MOVE_TOLERANCE, TERRAIN_BLOCKS_PAWN, placed_objects_)) return false;
	if (!is_game_started_) {
		const float dx = next[0] - START_DOME_CENTER[0];
		const float dy = next[1] - START_DOME_CENTER[1];
		const float dz = next[2] - START_DOME_CENTER[2];
		const float limit = START_DOME_RADIUS - CHARACTER_CAPSULE_RADIUS + MOVE_TOLERANCE;
		if (dx * dx + dy * dy + dz * dz > limit * limit) return false;
	}
	const float* previous = player.GetMoveState().location;
	return !collision_objects.IsSegmentBlockedByObjects({ previous[0], previous[1], previous[2] }, { next[0], next[1], next[2] }, TERRAIN_BLOCKS_PAWN, placed_objects_);
}

// 마지막으로 인정한 위치에 멈춘 상태로 되돌려 다른 플레이어에게 다음 틱에 중계하고, 본인에게는 그 위치로 옮기라고 보낸다.
void GameRoom::RollbackMove(Player& player, Session& session)
{
	S2C_MOVE_CORRECTION_PACKET correction_p;
	correction_p.header = { static_cast<std::uint16_t>(sizeof(correction_p)), S2C_MOVE_CORRECTION };
	correction_p.correction_id = player.StartMoveCorrection();
	std::memcpy(correction_p.location, player.GetMoveState().location, sizeof(correction_p.location));
	session.SendPacket(reinterpret_cast<char*>(&correction_p), correction_p.header.size);
}

// 전원이 로딩을 마치거나 로딩 제한 시간이 지나면 시작 대기를 알리고, 대기 시간이 지나면 게임 시작을 알린다.
void GameRoom::ProcessGameStart()
{
	if (is_game_started_) return;
	const auto send_to_playing = [&](char* packet, int packet_size) {
		for (auto& room_player : room_players_) {
			auto session = FindSession(room_player);
			if (session && room_player.GetRoomPlayerState() == RoomPlayerState::PLAY) session->SendPacket(packet, packet_size);
		}
	};
	if (game_start_ms_ == 0) {
		bool has_loading_player = false;
		for (auto& room_player : room_players_) {
			if (FindSession(room_player) && room_player.GetRoomPlayerState() == RoomPlayerState::LOADING) has_loading_player = true;
		}
		if (has_loading_player && tick_time_ms_ < loading_deadline_ms_) return;
		game_start_ms_ = tick_time_ms_ + GAME_READY_WAIT_MS;
		S2C_GAME_READY_PACKET ready_p;
		ready_p.header = { static_cast<std::uint16_t>(sizeof(ready_p)), S2C_GAME_READY };
		ready_p.remaining_ms = GAME_READY_WAIT_MS;
		send_to_playing(reinterpret_cast<char*>(&ready_p), ready_p.header.size);
		return;
	}
	if (tick_time_ms_ < game_start_ms_) return;
	is_game_started_ = true;
	S2C_GAME_START_PACKET game_start_p;
	game_start_p.header = { static_cast<std::uint16_t>(sizeof(game_start_p)), S2C_GAME_START };
	send_to_playing(reinterpret_cast<char*>(&game_start_p), game_start_p.header.size);
}

void GameRoom::FindNewHost()
{
	bool has_found_host = false;
	auto& room_players = room_players_;

	if (GetCurrentPlayerCount() != 0) {
		for (auto& room_player : room_players) {
			auto session = FindSession(room_player);
			if (!session) continue;
			host_id_ = session->GetSessionKey().player_id;
			if (room_state_ == RoomState::WAIT) room_player.SetRoomPlayerState(RoomPlayerState::WAIT); // 게임 중이 아닐 때, 호스트가 나갔을 때 레디 상태인 사람이 호스트가 되면, 레디 상태를 풀어줘야함
			has_found_host = true;
			break;
		}
	}

	if (has_found_host) {
		S2C_UPDATE_HOST_PACKET host_p;
		host_p.header.size = static_cast<std::uint16_t>(sizeof(host_p));
		host_p.header.type = S2C_UPDATE_HOST;
		host_p.new_host_id = host_id_;
		Broadcast(reinterpret_cast<char*>(&host_p));
	}

	else if (GetCurrentPlayerCount() == 0) {
		BeginRoomDelete();
	}
	else host_id_ = -1;
}

int GameRoom::FindHostIndex(int host_id)
{
	auto& room_players = room_players_;
	for (int i = 0; i < room_players.size(); ++i) {
		auto session = FindSession(room_players[i]);
		if (!session) continue;
		if (session->GetSessionKey().player_id == host_id) return i;
	}
	return -1;
}
