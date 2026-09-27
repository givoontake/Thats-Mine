#include <immintrin.h>
#include "LobbyThreadManager.h"
#include "ServerThreadManager.h"
#include "GameThreadManager.h"
#include "GameRoom.h"
#include "GameServer.h"
#include "char_buffer.h"
#include "SessionManager.h"
#include "LobbyPhaseContext.h"
#include "lobby_packets.h"
#include "packet_types.h"
#include "room_packets.h"
#include "room_lifecycle_tasks.h"

LobbyThreadManager::LobbyThreadManager(GameServer& game_server)
	: game_server_(game_server), login_manager_(game_server), matchmaker_(game_server)
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

void LobbyThreadManager::StartLobbyPhase(long long tick_time_ms)
{
	if (!game_server_.IsRunning()) return;
	for (const auto& lobby_thread : thread_objects_) {
		if (lobby_thread->state_.load() != LobbyThreadState::AVAILABLE) return;
	}

	tick_time_ms_.store(tick_time_ms);
	const auto phase_context = std::make_shared<LobbyPhaseContext>(lifecycle_tasks_.ClaimTaskCount(), THREAD_COUNT, tick_time_ms);
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

// 로비 목록에 있어야 하는 세션이다. 로그인했고 방에 있지 않은 활성 세션이면 전환 대기 중이어도 포함한다.
// 로그인 전 세션은 로그인 목록에 따로 있으므로 로비 방송을 받지 않는다.
bool LobbyThreadManager::ShouldBeListed(const SessionState& state)
{
	return state.life == LifeState::ACTIVE && state.mode != ModeState::NONE && state.mode != ModeState::ROOM && !LoginManager::IsLoginMode(state.mode);
}

// 세션의 로비 목록 등록 여부를 현재 세션 상태에 맞춘다. 어느 스레드에서 호출해도 된다.
// 로비 목록에 영향을 주는 상태 변경을 한 쪽은 상태를 바꾼 뒤 이 함수를 호출한다.
// 목록이 대상을 점유한 뒤 상태를 읽어 판단하므로, 호출 순서가 뒤섞여도 마지막 상태로 맞춰진다.
void LobbyThreadManager::SyncLobbyList(int session_index)
{
	lobby_list_.Sync(session_index, [this, session_index]() {
		auto* session = game_server_.GetSessionManager().FindSessionByIndex(session_index);
		return session && ShouldBeListed(session->GetState());
	});
}

// 로비 목록 인덱스의 세션을 찾는다. 비워졌거나, 다른 인덱스로 옮겨 갔거나, 수정 중인 인덱스는 걸러진다.
Session* LobbyThreadManager::FindLobbySession(int lobby_index)
{
	const std::int32_t session_index = lobby_list_.GetValue(lobby_index);
	if (session_index < 0) return nullptr;
	return game_server_.GetSessionManager().FindSessionByIndex(session_index);
}

// 로비 화면에 있는 세션이다. 빠른 매칭 중인 세션도 로비에 있으므로 포함한다.
bool LobbyThreadManager::IsInLobby(ModeState mode_state)
{
	return mode_state == ModeState::LOBBY || mode_state == ModeState::MATCHING;
}

// 받는 세션의 상태는 확인하지 않고 로비 목록 전원에게 보낸다. 전환 중인 세션이 받은 로비 패킷은 클라이언트가 현재 화면 상태에 맞지 않으면 거른다.
void LobbyThreadManager::BroadcastToLobby(char* packet)
{
	const int packet_size = static_cast<int>(reinterpret_cast<const PACKET_HEADER*>(packet)->size);
	for (int lobby_index = 0; lobby_index < lobby_list_.GetEnd(); ++lobby_index) {
		auto* player = FindLobbySession(lobby_index);
		if (player) player->SendPacket(packet, packet_size);
	}
}

void LobbyThreadManager::SendRoomList(Session& session)
{
	char packet_buffer[BUF_SIZE];
	int packet_size = 0;
	auto& game_thread_manager = game_server_.GetThreadManager().GetGameThreadManager();
	for (int room_index = 0; room_index < game_thread_manager.GetRoomIndexEnd(); ++room_index) {
		auto* room = game_thread_manager.GetRoomByIndex(room_index);
		RoomInfoSnapshot room_snapshot;
		if (!room || !room->TryGetRoomInfoSnapshot(room_snapshot)) continue;
		S2C_ROOM_INFO_PACKET info_p;
		if (room_snapshot.room_state != RoomState::WAIT && room_snapshot.room_state != RoomState::PLAY) continue;
		info_p.header.size = static_cast<std::uint16_t>(sizeof(info_p));
		info_p.header.type = S2C_ROOM_INFO;
		info_p.room_key = room_snapshot.room_key;
		info_p.max_player_count = static_cast<char>(room_snapshot.max_player_count);
		info_p.current_player_count = static_cast<char>(room_snapshot.current_player_count);
		memcpy(info_p.room_name, room_snapshot.room_name, sizeof(info_p.room_name));
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
		if (!IsInLobby(session.GetModeState())) return;
	}

	int packet_size = 0;
	char packet_buffer[BUF_SIZE];
	for (int lobby_index = 0; lobby_index < lobby_list_.GetEnd(); ++lobby_index) {
		auto* player = FindLobbySession(lobby_index);
		if (!player) continue;
		S2C_LOBBY_PLAYER_INFO_PACKET info_p;
		info_p.header.size = static_cast<std::uint16_t>(sizeof(info_p));
		info_p.header.type = S2C_LOBBY_PLAYER_INFO;
		{
			if (player->GetSessionKey().player_id == session.GetSessionKey().player_id) continue;
			if (IsInLobby(player->GetModeState())) {
				info_p.player_id = player->GetSessionKey().player_id;
				StringToCharBuf(player->GetNickname(), info_p.nickname, sizeof(info_p.nickname));
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
	// 로그인 전 세션의 패킷은 로그인 처리로만 보낸다. 아래의 로비 처리는 로그인한 세션만 받는다.
	if (LoginManager::IsLoginMode(session.GetModeState())) {
		login_manager_.ProcessPacket(packet, session);
		return;
	}
	switch (reinterpret_cast<PACKET_HEADER*>(packet)->type) {
	case C2S_MESSAGE: {
		if (!IsInLobby(session.GetModeState())) return;
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
		StringToCharBuf("", front_p.nickname, sizeof(front_p.nickname));
		memcpy(send_p, &front_p, sizeof(S2C_MESSAGE_PACKET));
		memcpy(send_p + sizeof(S2C_MESSAGE_PACKET), reinterpret_cast<char*>(recv_p) + sizeof(C2S_MESSAGE_PACKET), msg_size);
		BroadcastToLobby(send_p);
		delete[] send_p;
		break;
	}
	case C2S_ADD_PUBLIC_ROOM:
	case C2S_ADD_PRIVATE_ROOM: {
		// 전환 대기로 먼저 바꾼 뒤 방 생성 요청을 넣는다.
		const SessionKey session_key = session.GetSessionKey();
		if (!session.TryChangeMode(ModeState::LOBBY, ModeState::ROOM_PENDING)) break;
		const int packet_size = reinterpret_cast<PACKET_HEADER*>(packet)->size;
		const RoomLifecycleTaskType task_type = reinterpret_cast<PACKET_HEADER*>(packet)->type == C2S_ADD_PUBLIC_ROOM ? RoomLifecycleTaskType::CREATE_PUBLIC : RoomLifecycleTaskType::CREATE_PRIVATE;
		auto task = std::make_unique<RoomLifecycleTask>(task_type, session_key);
		task->packet.assign(packet, packet + packet_size);
		game_server_.EnqueueRoomLifecycleTask(std::move(task));
		break;
	}
	case C2S_JOIN_PUBLIC_ROOM: {
		C2S_JOIN_PUBLIC_ROOM_PACKET* join_p = reinterpret_cast<C2S_JOIN_PUBLIC_ROOM_PACKET*>(packet);
		const int result = game_server_.GetThreadManager().GetGameThreadManager().TryJoinRoom(session.GetSessionKey(), join_p->room_key, nullptr);
		if (result != SUCCESS) game_server_.SendError(session, result);
		break;
	}
	case C2S_JOIN_PRIVATE_ROOM: {
		C2S_JOIN_PRIVATE_ROOM_PACKET* join_p = reinterpret_cast<C2S_JOIN_PRIVATE_ROOM_PACKET*>(packet);
		const int result = game_server_.GetThreadManager().GetGameThreadManager().TryJoinRoom(session.GetSessionKey(), join_p->room_key, join_p->room_password);
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
		// 0은 기존 패킷의 정원 무관 요청이며, 현재는 20인 방만 매칭한다.
		if (matching_p->max_player_count != 0 && matching_p->max_player_count != MAX_ROOM_PLAYER_COUNT) {
			game_server_.SendError(session, ErrorCode::INVALID_REQUEST);
			break;
		}
		// 매칭 상태로 먼저 바꾼 뒤 매칭 풀에 등록한다. 매칭 중에도 로비가 담당하므로 로비 패킷은 계속 처리된다.
		if (!session.TryChangeMode(ModeState::LOBBY, ModeState::MATCHING)) {
			game_server_.SendError(session, ErrorCode::INVALID_REQUEST);
			break;
		}
		matchmaker_.Register(session.GetSessionKey().session_index, tick_time_ms_.load());
		break;
	}
	default:
		break;
	}
}

void LobbyThreadManager::ProcessTask(std::unique_ptr<LobbyTask> task)
{
	if (!task) return;
	switch (task->task_type) {
	case LobbyTaskType::SESSION_ADDED:
		login_manager_.AddSession(task->session_key);
		break;
	case LobbyTaskType::LOGIN_RESULT:
		login_manager_.ApplyLoginResult(task->session_key, task->login_result);
		break;
	case LobbyTaskType::RETURN_TO_LOBBY: {
		// 로비가 받는 쪽으로서 로비 복귀를 확정한다. 실패 조건 없이 LOBBY로 바꾼다.
		auto* session = game_server_.GetSessionManager().FindSession(task->session_key);
		if (!session || !session->TryChangeMode(ModeState::LOBBY_PENDING, ModeState::LOBBY)) return;
		if (task->result == SUCCESS) {
			// 방 퇴장·강퇴 완료를 본인에게 알린다.
			S2C_REMOVE_PLAYER_PACKET remove_p;
			remove_p.header.size = static_cast<std::uint16_t>(sizeof(remove_p));
			remove_p.header.type = S2C_REMOVE_PLAYER;
			remove_p.player_id = task->session_key.player_id;
			session->SendPacket(reinterpret_cast<char*>(&remove_p), remove_p.header.size);
		}
		else game_server_.SendError(*session, task->result);
		break;
	}
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
