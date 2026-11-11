#include <algorithm>
#include <immintrin.h>
#include "LobbyThreadManager.h"
#include "ServerThreadManager.h"
#include "GameThreadManager.h"
#include "GameRoom.h"
#include "GameServer.h"
#include "char_buffer.h"
#include "SessionManager.h"
#include "LobbyPhaseContext.h"
#include "database_packets.h"
#include "packet_types.h"
#include "room_packets.h"
#include "room_lifecycle_tasks.h"

LobbyThreadManager::LobbyThreadManager(GameServer& game_server)
	: game_server_(game_server)
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

void LobbyThreadManager::StartLobbyPhase(long long tick_time_ms, std::uint64_t tick_number)
{
	if (!game_server_.IsRunning()) return;
	for (const auto& lobby_thread : thread_objects_) {
		if (lobby_thread->state_.load() != LobbyThreadState::AVAILABLE) return;
	}

	const bool should_refresh_room_list = tick_time_ms - room_list_refresh_time_ms_ >= ROOM_LIST_REFRESH_INTERVAL_MS;
	if (should_refresh_room_list) room_list_refresh_time_ms_ = tick_time_ms;
	const auto phase_context = std::make_shared<LobbyPhaseContext>(lobby_tasks_.ClaimTaskCount(), request_tasks_.ClaimTaskCount(), tick_number, should_refresh_room_list);
	{
		std::lock_guard<std::mutex> lock(lobby_mutex_);
		for (const auto& lobby_thread : thread_objects_) {
			lobby_thread->state_.store(LobbyThreadState::PROCESSING);
			lobby_thread->phase_context_ = phase_context;
			lobby_thread->phase_.store(LobbyPhase::REQUEST_PROCESS);
		}
	}
	lobby_cv_.notify_all();
}

void LobbyThreadManager::EnqueueTask(LobbyTask task)
{
	lobby_tasks_.Enqueue(std::move(task));
}

// 로비 작업을 처리한다. 로비 단계에서 요청 처리 전에 스레드 하나만 호출한다.
// 입장 확정과 정리는 이 작업을 처리할 때까지 로비가 세션의 주체이므로, 찾은 세션이 처리하는 동안 바뀌지 않는다.
void LobbyThreadManager::ProcessTask(LobbyTask task)
{
	auto* session = game_server_.GetSessionManager().FindSession(task.session_key);
	if (!session) return;
	// 로비에 있던 세션의 연결 종료 정리 통보다. 정리 가능으로 바꾼다. 이 세션에 마지막으로 하는 일이다.
	// 앞선 요청 처리가 모두 끝난 뒤에 처리하므로, 그때 이 세션을 다루는 로비 스레드는 없다.
	if (task.task_type == LobbyTaskType::DISCONNECT) {
		session->SetDisconnectReady(ModeState::LOBBY);
		return;
	}
	ConfirmEnter(*session, task);
}

// 로비로 옮기는 요청을 받는 쪽으로서 확정한다. 소속을 바꾼 뒤 결과를 알린다.
// 그사이 연결 종료가 예약돼 바꾸지 못하면 정리 가능으로 바꾼다. 전환 대기 중에는 정리 통보가 오지 않으므로 받는 쪽이 정리한다.
void LobbyThreadManager::ConfirmEnter(Session& session, const LobbyTask& task)
{
	if (!session.TryChangeMode(ModeState::LOBBY_PENDING, ModeState::LOBBY)) {
		session.SetDisconnectReady(ModeState::LOBBY_PENDING);
		return;
	}

	const SessionKey session_key = session.GetSessionKey();
	switch (task.task_type) {
	case LobbyTaskType::LOGIN: {
		S2C_LOGIN_PACKET login_p{};
		login_p.header = { sizeof(login_p), S2C_LOGIN };
		login_p.player_id = session_key.player_id;
		StringToCharBuf(session.GetNickname(), login_p.nickname, sizeof(login_p.nickname));
		session.SendPacket(reinterpret_cast<const char*>(&login_p), sizeof(login_p));
		break;
	}
	// 방 퇴장·강퇴 완료를 본인에게 알린다. 로비가 확정한 뒤 알리므로, 알림을 받은 클라이언트의 다음 요청은 로비 소속일 때 도착한다.
	case LobbyTaskType::ROOM_EXIT: {
		S2C_REMOVE_PLAYER_PACKET remove_p;
		remove_p.header = { static_cast<std::uint16_t>(sizeof(remove_p)), S2C_REMOVE_PLAYER };
		remove_p.player_id = session_key.player_id;
		session.SendPacket(reinterpret_cast<const char*>(&remove_p), sizeof(remove_p));
		break;
	}
	case LobbyTaskType::MATCHING_CANCEL: {
		S2C_CANCEL_MATCHING_PACKET cancel_p;
		cancel_p.header = { static_cast<std::uint16_t>(sizeof(cancel_p)), S2C_CANCEL_MATCHING };
		session.SendPacket(reinterpret_cast<const char*>(&cancel_p), sizeof(cancel_p));
		break;
	}
	default:
		if (task.error_code != ErrorCode::SUCCESS) game_server_.SendError(session, task.error_code);
		break;
	}
}

// 전환 대기 중인 세션을 로비로 되돌리는 요청을 보낸다. 전환 요청을 받았지만 처리하지 못한 쪽(전환 대기의 주체)이 호출하며, 실패는 로비가 확정한 뒤 알린다.
// 연결 종료가 예약돼 바꾸지 못하면 정리 가능으로 바꾼다. 전환 대기 중에는 정리 통보가 오지 않으므로 호출한 쪽이 최종 소속이다.
void LobbyThreadManager::ReturnToLobby(Session& session, ModeState pending_mode, int error_code)
{
	if (session.TryChangeMode(pending_mode, ModeState::LOBBY_PENDING)) {
		EnqueueTask({ LobbyTaskType::ENTER, session.GetSessionKey(), error_code });
		return;
	}
	session.SetDisconnectReady(pending_mode);
}

// 방에 입장 요청을 보낸다. 방이 없으면 상태를 바꾸지 않고 알린다.
// 방이 삭제 단계에 들어가 요청을 받지 않으면 요청을 넘기지 못한 로비가 전환 대기의 주체로서 로비로 되돌린다.
// room_password는 패킷의 고정 길이 비밀번호 배열이며, 공개방 입장이면 nullptr이다.
void LobbyThreadManager::JoinRoom(Session& session, RoomKey room_key, const char* room_password)
{
	auto* room = game_server_.GetThreadManager().GetGameThreadManager().FindRoomByKey(room_key);
	if (!room) {
		game_server_.SendError(session, ErrorCode::ROOM_NOT_FOUND);
		return;
	}
	RoomTask task;
	task.task_type = RoomTaskType::JOIN;
	task.session_key = session.GetSessionKey();
	task.room_key = room_key;
	CopyCharBuf(task.room_password, sizeof(task.room_password), room_password, MAX_ROOM_PASSWORD_SIZE);
	// 목적지 방을 먼저 적고 전환 대기로 바꾼다. 전환 대기 중 받은 패킷은 그 방으로 간다.
	session.SetRoom(room->GetRoomIndex(), room_key);
	if (!session.TryChangeMode(ModeState::LOBBY, ModeState::ROOM_PENDING)) return;
	if (!room->AddRoomTask(std::move(task))) ReturnToLobby(session, ModeState::ROOM_PENDING, ErrorCode::ROOM_NOT_FOUND);
}

void LobbyThreadManager::EnqueueRequest(std::unique_ptr<SessionPacketTask> task)
{
	request_tasks_.Enqueue(std::move(task));
}

// 로비가 맡은 세션이다. 로비 요청을 처리할지 판단할 때 쓴다.
// 로비 소속은 로비 작업을 처리할 때만 생기고 정리되며, 요청 처리 중에는 로비 작업을 처리하지 않는다.
// 그래서 요청을 처리하는 동안 로비 소속인 세션이 정리되거나 다른 연결이 새로 로비 소속이 되는 일이 없다.
bool LobbyThreadManager::IsLobbySession(const SessionState& state)
{
	return state.life == LifeState::ACTIVE && state.mode == ModeState::LOBBY;
}

// 요청을 보낸 세션이 지금도 로비 소속일 때만 처리한다. 받은 뒤 다른 곳으로 갔거나 연결 종료가 예약됐으면 버린다.
// 로비가 처리하는 요청 종류인지. ProcessPacket이 처리하는 종류와 같아야 한다.
bool LobbyThreadManager::IsLobbyRequest(std::uint8_t packet_type)
{
	switch (packet_type) {
	case C2S_ADD_PUBLIC_ROOM:
	case C2S_ADD_PRIVATE_ROOM:
	case C2S_JOIN_PUBLIC_ROOM:
	case C2S_JOIN_PRIVATE_ROOM:
	case C2S_REQUEST_ROOM_LIST:
	case C2S_FAST_MATCHING:
		return true;
	default:
		return false;
	}
}

// 같은 틱에는 세션마다 요청 하나만 처리한다. 로비 스레드 여럿이 같은 세션의 요청을 동시에 처리하지 않게 하려는 것으로,
// 처리하기 전에 마지막 처리 틱을 이번 틱 번호로 바꾸고, 이전 값이 이번 틱이면 다른 스레드가 이미 처리했으므로 버린다.
// 세션 키와 로비 소속을 먼저 확인하므로, 바꾸는 칸은 언제나 요청을 보낸 그 세션의 칸이다.
// 로비 요청이 아닌 패킷(방을 나오기 전에 보낸 패킷, 로비에 있을 때 도착한 매칭 취소 등)은 틱을 차지하지 않고 버린다.
// 틱을 차지하면 같은 틱에 온 그 세션의 로비 요청이 버려지기 때문이다.
void LobbyThreadManager::ProcessRequest(std::unique_ptr<SessionPacketTask> task, std::uint64_t tick_number)
{
	if (!task) return;
	auto* session = game_server_.GetSessionManager().FindSession(task->session_key);
	if (!session || !IsLobbySession(session->GetState())) return;
	if (!IsLobbyRequest(reinterpret_cast<const PACKET_HEADER*>(task->packet.data())->type)) return;
	if (last_request_ticks_[task->session_key.session_index].exchange(tick_number) == tick_number) return;
	ProcessPacket(task->packet.data(), *session);
}

// 대기·게임 중인 방의 정보 패킷을 이어 붙인 새 목록을 만들어 교체한다. 이전 목록은 보내던 쪽이 다 쓰면 해제된다.
void LobbyThreadManager::RefreshRoomList()
{
	auto& game_thread_manager = game_server_.GetThreadManager().GetGameThreadManager();
	auto room_list = std::make_shared<std::vector<char>>();
	room_list->reserve(game_thread_manager.GetRoomIndexEnd() * sizeof(S2C_ROOM_INFO_PACKET));
	for (int room_index = 0; room_index < game_thread_manager.GetRoomIndexEnd(); ++room_index) {
		auto* room = game_thread_manager.GetRoomByIndex(room_index);
		RoomInfoSnapshot room_snapshot;
		if (!room || !room->TryGetRoomInfoSnapshot(room_snapshot)) continue;
		S2C_ROOM_INFO_PACKET info_p;
		if (room_snapshot.room_state != RoomState::WAIT && room_snapshot.room_state != RoomState::PLAY) continue;
		info_p.header.size = static_cast<std::uint16_t>(sizeof(info_p));
		info_p.header.type = S2C_ROOM_INFO;
		info_p.room_key = room_snapshot.room_key;
		info_p.current_player_count = static_cast<char>(room_snapshot.current_player_count);
		memcpy(info_p.room_name, room_snapshot.room_name, sizeof(info_p.room_name));
		info_p.is_private = room_snapshot.is_private;
		info_p.is_play = room_snapshot.room_state == RoomState::PLAY;

		const char* info_bytes = reinterpret_cast<const char*>(&info_p);
		room_list->insert(room_list->end(), info_bytes, info_bytes + sizeof(info_p));
	}
	room_list_.store(std::move(room_list));
}

// 마지막으로 만든 방 목록을 송신 버퍼 크기에 들어가는 패킷 수만큼씩 나눠 보낸다.
void LobbyThreadManager::SendRoomList(Session& session)
{
	const auto room_list = room_list_.load();
	if (!room_list) return;
	constexpr int SEND_SIZE = BUF_SIZE / sizeof(S2C_ROOM_INFO_PACKET) * sizeof(S2C_ROOM_INFO_PACKET);
	const int room_list_size = static_cast<int>(room_list->size());
	for (int offset = 0; offset < room_list_size; offset += SEND_SIZE)
		session.SendPacket(room_list->data() + offset, std::min(SEND_SIZE, room_list_size - offset));
}

// 로비를 떠나는 요청은 보내기 전에 전환 대기로 바꾼다. 바꾼 뒤에는 요청을 받는 쪽이 세션의 주체다.
// 연결 종료가 예약돼 바꾸지 못하면 로비 소속으로 남아 정리 통보로 정리한다.
// 로비 요청은 세션마다 한 틱에 하나만 처리하므로 바꾸지 못하는 것은 연결 종료가 예약됐을 때뿐이고, 그때는 알릴 곳이 없다.
void LobbyThreadManager::ProcessPacket(char* packet, Session& session)
{
	if (!packet) return;
	switch (reinterpret_cast<PACKET_HEADER*>(packet)->type) {
	case C2S_ADD_PUBLIC_ROOM:
	case C2S_ADD_PRIVATE_ROOM: {
		// 아직 방이 없음을 적고 전환 대기로 바꾼 뒤 방 생성 요청을 넣는다. 전환 대기 중 받은 패킷은 생성 처리로 간다.
		const SessionKey session_key = session.GetSessionKey();
		session.SetRoom(-1, 0);
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
		JoinRoom(session, join_p->room_key, nullptr);
		break;
	}
	case C2S_JOIN_PRIVATE_ROOM: {
		C2S_JOIN_PRIVATE_ROOM_PACKET* join_p = reinterpret_cast<C2S_JOIN_PRIVATE_ROOM_PACKET*>(packet);
		JoinRoom(session, join_p->room_key, join_p->room_password);
		break;
	}
	case C2S_REQUEST_ROOM_LIST:
		SendRoomList(session);
		break;
	case C2S_FAST_MATCHING: {
		// 매칭 전환 대기로 먼저 바꾼 뒤 매칭 스레드에 등록 요청을 넣는다. 매칭 스레드가 매칭 풀에 넣고 확정한다.
		const SessionKey session_key = session.GetSessionKey();
		if (!session.TryChangeMode(ModeState::LOBBY, ModeState::MATCHING_PENDING)) break;
		game_server_.GetThreadManager().GetMatchingThreadManager().GetMatchingThread().EnqueueTask({ MatchingTaskType::ENTER, session_key });
		break;
	}
	default:
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
