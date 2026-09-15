#include "LobbyThreadManager.h"
#include "ServerThreadManager.h"
#include "GameThreadManager.h"
#include "GameServer.h"
#include "LobbyPhaseContext.h"
#include "lobby_packets.h"
#include "packet_types.h"
#include "room_packets.h"
#include "room_lifecycle_tasks.h"
#include "database_packets.h"

LobbyThreadManager::LobbyThreadManager(GameServer& game_server)
	: game_server_(game_server), active_session_keys_(MAX_PLAYER_COUNT), session_index_registry_(MAX_PLAYER_COUNT)
{
}

void LobbyThreadManager::Start()
{
	thread_objects_.reserve(THREAD_COUNT);
	threads_.reserve(THREAD_COUNT);
	for (int i = 0; i < THREAD_COUNT; ++i) {
		thread_objects_.emplace_back(std::make_unique<LobbyThread>(*this));
		thread_objects_.back()->Start();
		threads_.emplace_back(&ServerThread::Run, thread_objects_.back().get());
	}
}

void LobbyThreadManager::StartLobbyPhase()
{
	if (!game_server_.IsRunning()) return;
	for (const auto& lobby_thread : thread_objects_) {
		if (lobby_thread->state_.load() != LobbyThreadState::AVAILABLE) return;
	}

	const auto phase_context = std::make_shared<LobbyPhaseContext>(lifecycle_tasks_.ClaimTaskCount(), THREAD_COUNT);
	{
		std::lock_guard<std::mutex> lock(lobby_mutex_);
		for (const auto& lobby_thread : thread_objects_) {
			lobby_thread->state_.store(LobbyThreadState::PROCESSING);
			lobby_thread->phase_context_ = phase_context;
			lobby_thread->phase_.store(LobbyPhase::SESSION_PROCESS);
		}
	}
	lobby_cv_.notify_all();
}

void LobbyThreadManager::Enqueue(std::unique_ptr<LobbyTask> task)
{
	lifecycle_tasks_.Enqueue(std::move(task));
}

LobbySession* LobbyThreadManager::GetLobbySession(int session_index)
{
	if (session_index < 0 || session_index >= MAX_PLAYER_COUNT) return nullptr;
	return &lobby_sessions_[session_index];
}

bool LobbyThreadManager::AddActiveSession(SessionKey session_key)
{
	if (!session_index_registry_.Register(session_key.session_id, session_key.session_index)) return false;
	if (active_session_keys_.Add(session_key.session_id)) return true;
	session_index_registry_.Unregister(session_key.session_id, session_key.session_index);
	return false;
}

void LobbyThreadManager::RemoveActiveSession(SessionKey session_key)
{
	active_session_keys_.Remove(session_key.session_id);
	session_index_registry_.Unregister(session_key.session_id, session_key.session_index);
}

Session* LobbyThreadManager::GetActiveSession(std::size_t active_session_index)
{
	const auto session_id = active_session_keys_.Get(active_session_index);
	if (!session_id) return nullptr;
	const auto session_index = session_index_registry_.Find(*session_id);
	if (!session_index) return nullptr;
	auto* session = game_server_.FindSessionByIndex(*session_index);
	if (!session || session->GetSessionKey().session_id != *session_id) return nullptr;
	return session;
}

bool LobbyThreadManager::BeginRoomTransition(SessionKey session_key)
{
	auto* session = game_server_.FindSession(session_key);
	if (!session || session->GetLifeState() != LifeState::ACTIVE || session->GetModeState() != ModeState::LOBBY) return false;
	auto* lobby_session = GetLobbySession(session_key.session_index);
	return lobby_session && lobby_session->TrySetPending(session_key);
}

void LobbyThreadManager::FindMatch(SessionKey session_key, int max_player_count)
{
	auto* session = game_server_.FindSession(session_key);
	if (!session) return;
	// 0은 기존 패킷의 정원 무관 요청이며, 현재는 20인 방만 검색한다.
	if (max_player_count != 0 && max_player_count != MAX_ROOM_PLAYER_COUNT) {
		game_server_.SendError(*session, ErrorCode::INVALID_REQUEST);
		return;
	}
	auto& game_thread_manager = game_server_.GetThreadManager().GetGameThreadManager();
	for (std::size_t active_room_index = 0; ; ++active_room_index) {
		// 방에 접근할 때는 무조건 Shared_ptr을 로드해서 참조 카운트를 늘려야 한다. 방이 삭제되더라도 안전하게 동작하기 위해서이다.
		// 단순히 널을 체크하고 들어가도 그 다음 내부 객체 접근 시 그 객체가 삭제되었을 수 있다.
		auto room_sp = game_thread_manager.GetActiveRoom(active_room_index);
		if (!room_sp) break;
		if (room_sp->IsPrivate()) continue;
		const RoomInfoSnapshot snapshot = room_sp->GetRoomInfoSnapshot();
		if (snapshot.room_state != RoomState::WAIT || snapshot.current_player_count >= MAX_ROOM_PLAYER_COUNT) continue;
		const int result = game_thread_manager.TryJoinRoom(session_key, room_sp->GetRoomKey(), "", MAX_ROOM_PLAYER_COUNT);
		if (result == SUCCESS) return;
		if (result == ErrorCode::INVALID_REQUEST || result == ErrorCode::SERVER_ERROR) {
			game_server_.SendError(*session, result);
			return;
		}
	}
	game_server_.SendError(*session, ErrorCode::NOT_FOUND_JOINABLE_ROOM);
}

void LobbyThreadManager::BroadcastToLobby(char* packet)
{
	const int packet_size = static_cast<int>(reinterpret_cast<const PACKET_HEADER*>(packet)->size);
	for (std::size_t active_session_index = 0; ; ++active_session_index) {
		auto* player = GetActiveSession(active_session_index);
		if (!player) break;
		if (player && player->GetModeState() == ModeState::LOBBY) {
			player->SendPacket(packet, packet_size);
		}
	}
}

void LobbyThreadManager::SendRoomList(Session& session)
{
	char packet_buffer[BUF_SIZE];
	int packet_size = 0;
	auto& game_thread_manager = game_server_.GetThreadManager().GetGameThreadManager();
	for (std::size_t active_room_index = 0; ; ++active_room_index) {
		auto room_sp = game_thread_manager.GetActiveRoom(active_room_index);
		if (!room_sp) break;
		S2C_ROOM_INFO_PACKET info_p;
		RoomInfoSnapshot room_snapshot = room_sp->GetRoomInfoSnapshot();
		if (room_snapshot.room_state != RoomState::WAIT && room_snapshot.room_state != RoomState::PLAY) continue;
		info_p.header.size = static_cast<std::uint16_t>(sizeof(info_p));
		info_p.header.type = S2C_ROOM_INFO;
		info_p.room_key = room_snapshot.room_key;
		info_p.max_player_count = room_snapshot.max_player_count;
		info_p.current_player_count = room_snapshot.current_player_count;
		game_server_.StringToCharBuf(room_snapshot.room_name, info_p.room_name, sizeof(info_p.room_name));
		info_p.is_private = room_snapshot.is_private;
		info_p.is_play = room_snapshot.room_state == RoomState::PLAY;

		if (packet_size + sizeof(info_p) > BUF_SIZE) {
			session.SendPacket(reinterpret_cast<char*>(packet_buffer), packet_size);
			packet_size = 0;
		}

		memcpy(packet_buffer + packet_size, &info_p, sizeof(info_p));
		packet_size += sizeof(info_p);
	}
	session.SendPacket(reinterpret_cast<char*>(packet_buffer), packet_size);
}

void LobbyThreadManager::SendLobbyPlayerList(Session& session)
{
	{
		// 로비에 있는 플레이어 목록만 조회한다.
		if (session.GetModeState() != ModeState::LOBBY) return;
	}

	int packet_size = 0;
	char packet_buffer[BUF_SIZE];
	for (std::size_t active_session_index = 0; ; ++active_session_index) {
		auto* player = GetActiveSession(active_session_index);
		if (!player) break;
		S2C_LOBBY_PLAYER_INFO_PACKET info_p;
		info_p.header.size = static_cast<std::uint16_t>(sizeof(info_p));
		info_p.header.type = S2C_LOBBY_PLAYER_INFO;
		{
			if (player->GetSessionKey().player_id == session.GetSessionKey().player_id) continue;
			if (player->GetModeState() == ModeState::LOBBY) {
				info_p.player_id = player->GetSessionKey().player_id;
				game_server_.StringToCharBuf(player->GetNickname(), info_p.nickname, sizeof(info_p.nickname));
			}
			else continue;
		}

		if (packet_size + sizeof(info_p) > BUF_SIZE) {
			session.SendPacket(reinterpret_cast<char*>(packet_buffer), packet_size);
			packet_size = 0;
		}

		memcpy(packet_buffer + packet_size, &info_p, sizeof(info_p));
		packet_size += sizeof(info_p);
	}
	session.SendPacket(reinterpret_cast<char*>(packet_buffer), packet_size);
}

void LobbyThreadManager::ProcessPacket(char* packet, Session& session)
{
	if (!packet) return;
	switch (reinterpret_cast<PACKET_HEADER*>(packet)->type) {
	case C2S_LOGIN: {
		if (session.GetModeState() != ModeState::LOGIN) return;
		const auto* request = reinterpret_cast<const C2S_LOGIN_PACKET*>(packet);
		DBLoginTask task{session.GetSessionKey(), game_server_.CharBufToString(request->login_id, sizeof(request->login_id)), game_server_.CharBufToString(request->login_password, sizeof(request->login_password))};
		game_server_.GetThreadManager().GetDBThreadManager().Enqueue(std::move(task));
		break;
	}
	case C2S_MESSAGE: {
		if (session.GetModeState() != ModeState::LOBBY) return;
		C2S_MESSAGE_PACKET* recv_p = reinterpret_cast<C2S_MESSAGE_PACKET*>(packet);
		int msg_size = recv_p->header.size - sizeof(C2S_MESSAGE_PACKET);
		if (msg_size == 0) return;
		int send_p_size = sizeof(S2C_MESSAGE_PACKET) + msg_size;
		const SessionKey session_key = session.GetSessionKey();

		char* send_p = new char[send_p_size];
		S2C_MESSAGE_PACKET front_p;
		front_p.header.size = static_cast<std::uint16_t>(send_p_size);
		front_p.header.type = S2C_MESSAGE;
		front_p.player_id = session_key.player_id;
		game_server_.StringToCharBuf("", front_p.nickname, sizeof(front_p.nickname));
		memcpy(send_p, &front_p, sizeof(S2C_MESSAGE_PACKET));
		memcpy(send_p + sizeof(S2C_MESSAGE_PACKET), reinterpret_cast<char*>(recv_p) + sizeof(C2S_MESSAGE_PACKET), msg_size);
		BroadcastToLobby(send_p);
		delete[] send_p;
		break;
	}
	case C2S_ADD_PUBLIC_ROOM:
	case C2S_ADD_PRIVATE_ROOM: {
		const SessionKey session_key = session.GetSessionKey();
		if (!BeginRoomTransition(session_key)) break;
		const int packet_size = reinterpret_cast<PACKET_HEADER*>(packet)->size;
		const RoomLifecycleTaskType task_type = reinterpret_cast<PACKET_HEADER*>(packet)->type == C2S_ADD_PUBLIC_ROOM ? RoomLifecycleTaskType::CREATE_PUBLIC : RoomLifecycleTaskType::CREATE_PRIVATE;
		auto task = std::make_unique<RoomLifecycleTask>(task_type, session_key);
		task->packet.assign(packet, packet + packet_size);
		game_server_.EnqueueRoomLifecycleTask(std::move(task));
		break;
	}
	case C2S_JOIN_PUBLIC_ROOM: {
		C2S_JOIN_PUBLIC_ROOM_PACKET* join_p = reinterpret_cast<C2S_JOIN_PUBLIC_ROOM_PACKET*>(packet);
		const int result = game_server_.GetThreadManager().GetGameThreadManager().TryJoinRoom(session.GetSessionKey(), join_p->room_key, "");
		if (result != SUCCESS) game_server_.SendError(session, result);
		break;
	}
	case C2S_JOIN_PRIVATE_ROOM: {
		C2S_JOIN_PRIVATE_ROOM_PACKET* join_p = reinterpret_cast<C2S_JOIN_PRIVATE_ROOM_PACKET*>(packet);
		const int result = game_server_.GetThreadManager().GetGameThreadManager().TryJoinRoom(session.GetSessionKey(), join_p->room_key, game_server_.CharBufToString(join_p->room_password, sizeof(join_p->room_password)));
		if (result != SUCCESS) game_server_.SendError(session, result);
		break;
	}
	case C2S_REQUEST_ROOM_LIST:
		SendRoomList(session);
		break;
	case C2S_REQUEST_LOBBY_PLAYER_LIST:
		SendLobbyPlayerList(session);
		break;
	case C2S_FAST_MATCHING: {
		C2S_FAST_MATCHING_PACKET* matching_p = reinterpret_cast<C2S_FAST_MATCHING_PACKET*>(packet);
		FindMatch(session.GetSessionKey(), matching_p->max_player_count);
		break;
	}
	default:
		break;
	}
}

void LobbyThreadManager::ProcessTask(std::unique_ptr<LobbyTask> task)
{
	if (!task) return;
	auto* lobby_session = GetLobbySession(task->session_key.session_index);
	switch (task->task_type) {
	case LobbyTaskType::ADD_SESSION: {
		auto* session = game_server_.FindSession(task->session_key);
		if (!lobby_session || !session || session->GetLifeState() != LifeState::ACTIVE) return;
		const ModeState mode_state = session->GetModeState();
		if (mode_state != ModeState::LOGIN && mode_state != ModeState::LOBBY) return;
		if (lobby_session->TryPrepare(task->session_key) && lobby_session->Activate(task->session_key) && !AddActiveSession(task->session_key))
			lobby_session->Clear(task->session_key);
		break;
	}
	case LobbyTaskType::ROOM_TRANSITION_RESULT: {
		if (!lobby_session) return;
		if (task->result == SUCCESS) {
			if (lobby_session->Clear(task->session_key)) RemoveActiveSession(task->session_key);
			return;
		}
		auto* session = game_server_.FindSession(task->session_key);
		if (!session) {
			if (lobby_session->Clear(task->session_key)) RemoveActiveSession(task->session_key);
			return;
		}
		if (!lobby_session->Activate(task->session_key)) return;
		if (task->matching_max_player_count >= 0) FindMatch(task->session_key, task->matching_max_player_count);
		else game_server_.SendError(*session, task->result);
		break;
	}
	case LobbyTaskType::ENTER_LOBBY: {
		int result = ErrorCode::INVALID_REQUEST;
		auto* session = game_server_.FindSession(task->session_key);
		bool is_processing = false;
		if (lobby_session && session && session->GetLifeState() == LifeState::ACTIVE) {
			if (!session->TryStartTaskProcessing()) {
				Enqueue(std::move(task));
				return;
			}
			is_processing = true;
			const RoomSnapshot room_snapshot = session->GetRoomSnapshot();
			if (room_snapshot.mode_state == ModeState::ROOM && room_snapshot.room_index == task->room_index && lobby_session->TryPrepare(task->session_key)) {
				if (session->TrySetLobbyMode(task->session_key)) result = SUCCESS;
				else lobby_session->Clear(task->session_key);
			}
		}

		if (result == SUCCESS && lobby_session->Activate(task->session_key)) {
			if (AddActiveSession(task->session_key)) {
				S2C_REMOVE_PLAYER_PACKET remove_p;
				remove_p.header.size = static_cast<std::uint16_t>(sizeof(remove_p));
				remove_p.header.type = S2C_REMOVE_PLAYER;
				remove_p.player_id = task->session_key.player_id;
				session->SendPacket(reinterpret_cast<char*>(&remove_p), remove_p.header.size);
			}
			else lobby_session->Clear(task->session_key);
		}

		if (is_processing) session->CompleteTaskProcessing();
		break;
	}
	case LobbyTaskType::DISCONNECT:
		if (!game_server_.FinalizeDisconnect(task->session_key)) Enqueue(std::move(task));
		break;
	}
}

void LobbyThreadManager::Close()
{
	for (auto& thread_object : thread_objects_)
		thread_object->Close();
}

void LobbyThreadManager::Join()
{
	for (auto& thread : threads_)
		thread.join();
}
