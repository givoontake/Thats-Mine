#include <random>
#include <utility>
#include "GameRoom.h"
#include "GameServer.h"
#include "packet_types.h"
#include "common_packets.h"
#include "room_packets.h"
#include "lobby_tasks.h"
#include "room_lifecycle_tasks.h"

GameRoom::GameRoom(GameServer* server, PublicRoomInitData data)
{
	// 생성이 끝난 방만 방 배열에 게시된다.
	server_ = server;
	room_name_ = std::move(data.room_name);
	room_password_.clear();
	room_index_ = data.room_index;
	room_key_ = data.room_key;
	room_state_.store(RoomState::EMPTY);

	current_player_count_.store(0);
}

GameRoom::GameRoom(GameServer* server, PrivateRoomInitData data)
{
	server_ = server;
	room_name_ = std::move(data.room_name);
	room_password_ = std::move(data.room_password);
	room_index_ = data.room_index;
	room_key_ = data.room_key;
	room_state_.store(RoomState::EMPTY);
	current_player_count_.store(0);
}

GameRoom::~GameRoom()
{
}

int GameRoom::GetCurrentPlayerCount() const
{
	return static_cast<int>(current_player_count_.load());
}

bool GameRoom::InitHostSession(Session& session, SessionKey session_key)
{
	auto& room_players = room_players_;
	if (room_players.empty()) return false;
	if (room_players[0].HasSession()) return false;
	if (!room_players[0].InitPlayer(session, session_key, room_index_)) return false;
	current_player_count_.store(1);
	return true;
}

Session* GameRoom::FindSession(const Player& player) const
{
	if (!player.IsActive()) return nullptr;
	return server_->FindSession(player.GetSessionKey());
}

Player* GameRoom::FindPlayer(SessionKey session_key)
{
	auto& room_players = room_players_;
	for (auto& room_player : room_players) {
		if (room_player.MatchesSessionKey(session_key)) return &room_player;
	}
	return nullptr;
}

SessionKey GameRoom::ClearPlayer(Player& player)
{
	const SessionKey session_key = player.GetSessionKey();
	auto* session = server_->FindSession(session_key);
	player.ClearPlayer();
	if (!session) return {};
	const LifeState life_state = session->GetLifeState();
	return life_state == LifeState::DISCONNECT_PENDING || life_state == LifeState::DISCONNECTING ? session_key : SessionKey{};
}

bool GameRoom::RequestLobbyTransition(SessionKey session_key, RoomExitType exit_type)
{
	auto* player = FindPlayer(session_key);
	if (!player || !player->TrySetPending(session_key)) return false;
	CompletePlayerRemoval(session_key, exit_type);
	auto task = std::make_unique<LobbyTask>(LobbyTaskType::ENTER_LOBBY, session_key);
	task->room_index = room_index_;
	server_->EnqueueLobbyTask(std::move(task));
	return true;
}

bool GameRoom::ActivatePlayer(SessionKey session_key)
{
	auto* player = FindPlayer(session_key);
	return player && player->Activate(session_key);
}
bool GameRoom::AddHostSession(Session& session, SessionKey session_key)
{
	if (!InitHostSession(session, session_key)) return false;
	host_id_ = session.GetSessionKey().player_id;
	return true;
}

int GameRoom::AddPlayer(Session& new_session, SessionKey session_key, const std::string& room_password)
{
	if (new_session.GetLifeState() != LifeState::ACTIVE || new_session.GetModeState() != ModeState::LOBBY) return ErrorCode::INVALID_REQUEST;
	if (room_password_ != room_password) return ErrorCode::ROOM_INVALID_PASSWORD;
	const RoomState room_state = room_state_.load();
	if (room_state == RoomState::PLAY) return ErrorCode::ROOM_IN_GAME;
	if (room_state != RoomState::WAIT) return ErrorCode::ROOM_NOT_FOUND;
	auto& room_players = room_players_;
	if (FindPlayer(session_key)) return ErrorCode::INVALID_REQUEST;
	int result = ErrorCode::ROOM_FULL;
	int added_slot = -1;
	for (int i = 0; i < room_players.size(); ++i) {
		auto& room_player = room_players[i];
		if (room_player.HasSession()) continue;
		if (!room_player.InitPlayer(new_session, session_key, room_index_)) return ErrorCode::INVALID_REQUEST;
		++current_player_count_;
		result = SUCCESS;
		added_slot = i;
		break;
	}

	if (result == SUCCESS) {
		if (room_password_.empty()) {
			S2C_ADD_PUBLIC_ROOM_PACKET public_p;
			public_p.header.size = static_cast<std::uint16_t>(sizeof(public_p));
			public_p.header.type = S2C_ADD_PUBLIC_ROOM;
			public_p.room_key = room_key_;
			public_p.max_player_count = MAX_ROOM_PLAYER_COUNT;
			server_->StringToCharBuf(room_name_, public_p.room_name, sizeof(public_p.room_name));
			new_session.SendPacket(reinterpret_cast<char*>(&public_p), public_p.header.size);
		}
		else {
			S2C_ADD_PRIVATE_ROOM_PACKET private_p;
			private_p.header.size = static_cast<std::uint16_t>(sizeof(private_p));
			private_p.header.type = S2C_ADD_PRIVATE_ROOM;
			private_p.room_key = room_key_;
			private_p.max_player_count = MAX_ROOM_PLAYER_COUNT;
			server_->StringToCharBuf(room_name_, private_p.room_name, sizeof(private_p.room_name));
			server_->StringToCharBuf(room_password_, private_p.room_password, sizeof(private_p.room_password));
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
			server_->StringToCharBuf(new_session.GetNickname(), add_p.nickname, sizeof(add_p.nickname));
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
			server_->StringToCharBuf(session->GetNickname(), add_p.nickname, sizeof(add_p.nickname));
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
		return result;
	}

	return result;
}

RoomInfoSnapshot GameRoom::GetRoomInfoSnapshot()
{
	RoomInfoSnapshot snapshot;
	snapshot.room_key = room_key_;
	snapshot.max_player_count = static_cast<int>(MAX_ROOM_PLAYER_COUNT);
	snapshot.current_player_count = static_cast<int>(current_player_count_.load());
	snapshot.room_name = room_name_;
	snapshot.is_private = !room_password_.empty();
	snapshot.room_state = room_state_.load();
	return snapshot;
}

bool GameRoom::IsPlayerInRoom(const Session& session) const
{
	const RoomSnapshot snapshot = session.GetRoomSnapshot();
	return snapshot.mode_state == ModeState::ROOM && snapshot.room_index == room_index_;
}

void GameRoom::HandlePacket(char* packet, Session& request_session)
{
	switch (reinterpret_cast<PACKET_HEADER*>(packet)->type) {
	case C2S_READY: {
		RoomTask task;
		task.task_type = RoomTaskType::READY;
		task.session_key = request_session.GetSessionKey();
		AddRoomTask(std::move(task));
		break;
	}
	case C2S_KICK: {
		C2S_KICK_PACKET* kick_p = reinterpret_cast<C2S_KICK_PACKET*>(packet);
		RoomTask task;
		task.task_type = RoomTaskType::KICK;
		task.session_key = request_session.GetSessionKey();
		task.target_player_id = kick_p->kick_player_id;
		AddRoomTask(std::move(task));
		break;
	}
	case C2S_REMOVE_PLAYER: {
		RequestLobbyTransition(request_session.GetSessionKey());
		break;
	}
	case C2S_START: {
		RoomTask task;
		task.task_type = RoomTaskType::START;
		task.session_key = request_session.GetSessionKey();
		AddRoomTask(std::move(task));
		break;
	}
	default:
		break;
	}
}

bool GameRoom::AddRoomTask(RoomTask task)
{
	std::lock_guard<std::mutex> lock(room_mutex_);
	const RoomState state = room_state_.load();
	if (state != RoomState::WAIT && state != RoomState::PLAY) return false;
	room_tasks_.Enqueue(std::move(task));
	return true;
}

void GameRoom::ProcessSessionTasks()
{
	auto& room_players = room_players_;
	for (auto& room_player : room_players) {
		if (!room_player.IsActive()) continue;
		const SessionKey session_key = room_player.GetSessionKey();
		auto* session = server_->FindSession(session_key);
		if (!session || session->GetLifeState() != LifeState::ACTIVE) continue;
		RoomSnapshot room_snapshot = session->GetRoomSnapshot();
		if (room_snapshot.mode_state != ModeState::ROOM || room_snapshot.room_index != room_index_) continue;
		if (!session->TryStartTaskProcessing()) continue;

		room_snapshot = session->GetRoomSnapshot();
		if (!room_player.IsActive() || !room_player.MatchesSessionKey(session_key) || !session->MatchesSessionKey(session_key) || session->GetLifeState() != LifeState::ACTIVE || room_snapshot.mode_state != ModeState::ROOM || room_snapshot.room_index != room_index_) {
			session->CompleteTaskProcessing();
			continue;
		}

		const std::size_t task_count = session->ClaimTaskCount();
		for (std::size_t i = 0; i < task_count; ++i) {
			const SessionTaskProcessResult result = server_->ProcessSessionTask(*session, session->DequeueTask());
			if (result == SessionTaskProcessResult::DISCARD_REMAINING) {
				for (++i; i < task_count; ++i) session->DequeueTask();
				session->DiscardTasks();
				break;
			}

			room_snapshot = session->GetRoomSnapshot();
			if (result == SessionTaskProcessResult::RESTORE_REMAINING || !room_player.IsActive() || !room_player.MatchesSessionKey(session_key) || session->GetLifeState() != LifeState::ACTIVE || room_snapshot.mode_state != ModeState::ROOM || room_snapshot.room_index != room_index_) {
				session->RestoreClaimedTaskCount(task_count - i - 1);
				break;
			}
		}
		session->CompleteTaskProcessing();
	}
}

bool GameRoom::TryProcessRoomTask(const RoomTask& task)
{
	if (task.task_type == RoomTaskType::JOIN) return TryProcessJoinTask(task);

	auto* session = server_->FindSession(task.session_key);
	if (!session) {
		if (task.task_type == RoomTaskType::REMOVE_PLAYER) RemovePlayer(task.session_key);
		return true;
	}
	if (!session->TryStartTaskProcessing()) return false;
	if (!session->MatchesSessionKey(task.session_key)) {
		session->CompleteTaskProcessing();
		return true;
	}
	if (task.task_type == RoomTaskType::REMOVE_PLAYER) {
		RemovePlayer(task.session_key);
		session->CompleteTaskProcessing();
		return true;
	}
	auto* player = FindPlayer(task.session_key);
	if (!player || !player->IsActive()) {
		session->CompleteTaskProcessing();
		return true;
	}
	const RoomSnapshot room_snapshot = session->GetRoomSnapshot();
	if (session->GetLifeState() != LifeState::ACTIVE || room_snapshot.mode_state != ModeState::ROOM || room_snapshot.room_index != room_index_) {
		session->CompleteTaskProcessing();
		return true;
	}
	const bool is_processed = ProcessSpecificRoomTask(task);
	session->CompleteTaskProcessing();
	return is_processed;
}

bool GameRoom::TryProcessJoinTask(const RoomTask& task)
{
	int result = ErrorCode::INVALID_REQUEST;
	auto* session = server_->FindSession(task.session_key);
	if (session) {
		if (!session->TryStartTaskProcessing()) return false;
		if (session->MatchesSessionKey(task.session_key) && session->GetLifeState() == LifeState::ACTIVE && session->GetModeState() == ModeState::LOBBY) {
			result = AddPlayer(*session, task.session_key, task.room_password);
			if (result == SUCCESS) ActivatePlayer(task.session_key);
		}
		session->CompleteTaskProcessing();
	}

	auto lobby_task = std::make_unique<LobbyTask>(LobbyTaskType::ROOM_TRANSITION_RESULT, task.session_key);
	lobby_task->result = result;
	lobby_task->matching_max_player_count = task.matching_max_player_count;
	server_->EnqueueLobbyTask(std::move(lobby_task));
	return true;
}

void GameRoom::ProcessRoomTasks()
{
	if (pending_room_task_) {
		if (!TryProcessRoomTask(*pending_room_task_)) return;
		pending_room_task_.reset();
	}

	const std::size_t task_count = room_tasks_.ClaimTaskCount();
	for (std::size_t i = 0; i < task_count; ++i) {
		RoomTask task = room_tasks_.Dequeue();
		if (TryProcessRoomTask(task)) continue;
		pending_room_task_ = std::move(task);
		room_tasks_.RestoreClaimedTaskCount(task_count - i - 1);
		return;
	}
	TryPostRoomDelete();
}

void GameRoom::ProcessRoomTick(long long tick_time_ms)
{
	ProcessRoomTasks();
	ProcessSessionTasks();
	ProcessRoomTasks();
	if (room_state_.load() == RoomState::PLAY) ProcessGameTick(tick_time_ms);
}

void GameRoom::CompleteRoomInitialization()
{
	auto& room_players = room_players_;
	if (!room_players.empty() && room_players[0].HasSession()) room_players[0].Activate(room_players[0].GetSessionKey());
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
	server_->EnqueueRoomLifecycleTask(std::make_unique<RoomLifecycleTask>(room_index_));
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

void GameRoom::ClearRoom()
{
	auto& room_players = room_players_;
	std::vector<SessionKey> disconnected_session_keys(room_players.size());
	int disconnected_session_count = 0;
	for (auto& room_player : room_players) {
		const SessionKey session_key = ClearPlayer(room_player);
		if (session_key.session_index >= 0) disconnected_session_keys[disconnected_session_count++] = session_key;
	}
	current_player_count_.store(0);
	StoreRoomState(RoomState::EMPTY);
	for (int i = 0; i < disconnected_session_count; ++i)
		server_->CompleteRoomDisconnect(disconnected_session_keys[i]);
}

void GameRoom::StoreRoomState(RoomState new_state)
{
	room_state_.store(new_state);
}

bool GameRoom::TryChangeRoomState(RoomState expected, RoomState desired)
{
	return room_state_.compare_exchange_strong(expected, desired);
}


void GameRoom::RemovePlayer(SessionKey session_key)
{
	auto* session = server_->FindSession(session_key);
	if (session && session->GetLifeState() == LifeState::ACTIVE) {
		RequestLobbyTransition(session_key);
		return;
	}
	CompletePlayerRemoval(session_key, RoomExitType::LEAVE);
}

void GameRoom::CompletePlayerRemoval(SessionKey session_key, RoomExitType exit_type)
{
	auto& room_players = room_players_;
	for (auto& room_player : room_players) {
		if (room_player.MatchesSessionKey(session_key)) { // 삭제할 세션 검색
			const int player_id = room_player.GetSessionKey().player_id;
			if (current_player_count_.load() == 1) BeginRoomDelete();
			S2C_REMOVE_PLAYER_PACKET p;
			p.header.size = static_cast<std::uint16_t>(sizeof(p));
			p.header.type = S2C_REMOVE_PLAYER;
			p.player_id = player_id;

			Broadcast(reinterpret_cast<char*>(&p));
			if (exit_type == RoomExitType::KICK) {
				auto* session = server_->FindSession(session_key);
				if (session) {
					S2C_INFO_PACKET info_p;
					info_p.header.size = static_cast<std::uint16_t>(sizeof(info_p));
					info_p.header.type = S2C_INFO;
					info_p.info_code = InfoCode::KICKED;
					session->SendPacket(reinterpret_cast<char*>(&info_p), info_p.header.size);
				}
			}
			const SessionKey disconnected_session_key = ClearPlayer(room_player);
			--current_player_count_;
			if (player_id == host_id_) FindNewHost(); // 여기서 찾기 및 못찾을 경우 삭제까지 같이 함
			if (disconnected_session_key.session_index >= 0) server_->CompleteRoomDisconnect(disconnected_session_key);
			break;
		}
	}
}

void GameRoom::SendCreateRoom(Session& session)
{
	if (room_password_.empty()) {
		S2C_ADD_PUBLIC_ROOM_PACKET public_p;
		public_p.header.size = static_cast<std::uint16_t>(sizeof(public_p));
		public_p.header.type = S2C_ADD_PUBLIC_ROOM;
		public_p.room_key = room_key_;
		public_p.max_player_count = MAX_ROOM_PLAYER_COUNT;
		server_->StringToCharBuf(room_name_, public_p.room_name, sizeof(public_p.room_name));
		session.SendPacket(reinterpret_cast<char*>(&public_p), public_p.header.size);
	}
	else {
		S2C_ADD_PRIVATE_ROOM_PACKET private_p;
		private_p.header.size = static_cast<std::uint16_t>(sizeof(private_p));
		private_p.header.type = S2C_ADD_PRIVATE_ROOM;
		private_p.room_key = room_key_;
		private_p.max_player_count = MAX_ROOM_PLAYER_COUNT;
		server_->StringToCharBuf(room_name_, private_p.room_name, sizeof(private_p.room_name));
		server_->StringToCharBuf(room_password_, private_p.room_password, sizeof(private_p.room_password));
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

bool GameRoom::KickPlayer(int requester_id, int kick_player_id)
{
	if (requester_id != host_id_) return true;
	if (kick_player_id == host_id_) return true;
	auto& room_players = room_players_;

	for (auto& room_player : room_players) {
		if (room_player.GetSessionKey().player_id != kick_player_id) continue;
		auto session = FindSession(room_player);
		if (!session) continue;
		if (!session->TryStartTaskProcessing()) return false;
		if (!session->MatchesSessionKey(room_player.GetSessionKey()) || !IsPlayerInRoom(*session)) {
			session->CompleteTaskProcessing();
			return true;
		}

		RequestLobbyTransition(room_player.GetSessionKey(), RoomExitType::KICK);
		session->CompleteTaskProcessing();
		return true;
	}
	return true;
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
	// 시작 조건을 충족한 플레이어를 게임 상태로 전환한다.
	for (auto& room_player : room_players) {
		if (FindSession(room_player)) room_player.SetRoomPlayerState(RoomPlayerState::PLAY);
	}
}

bool GameRoom::ProcessSpecificRoomTask(const RoomTask& task)
{
	auto session = server_->FindSession(task.session_key);
	if (!session) return true;
	switch (task.task_type) {
	case RoomTaskType::READY:
		if (IsPlayerInRoom(*session)) TogglePlayerReady(session->GetSessionKey().player_id);
		break;
	case RoomTaskType::KICK:
		if (IsPlayerInRoom(*session)) return KickPlayer(session->GetSessionKey().player_id, task.target_player_id);
		break;
	case RoomTaskType::START:
		if (IsPlayerInRoom(*session)) StartGame(session->GetSessionKey().player_id);
		break;
	default:
		break;
	}
	return true;
}

void GameRoom::ProcessGameTick(long long)
{
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