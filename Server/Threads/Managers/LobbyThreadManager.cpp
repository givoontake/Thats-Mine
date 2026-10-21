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
#include "packet_types.h"
#include "room_packets.h"
#include "room_lifecycle_tasks.h"

LobbyThreadManager::LobbyThreadManager(GameServer& game_server)
	: game_server_(game_server), matchmaker_(game_server)
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
	const bool should_refresh_room_list = tick_time_ms - room_list_refresh_time_ms_ >= ROOM_LIST_REFRESH_INTERVAL_MS;
	if (should_refresh_room_list) room_list_refresh_time_ms_ = tick_time_ms;
	const auto phase_context = std::make_shared<LobbyPhaseContext>(request_tasks_.ClaimTaskCount(), THREAD_COUNT, tick_time_ms, should_refresh_room_list);
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

void LobbyThreadManager::EnqueueRequest(std::unique_ptr<SessionPacketTask> task)
{
	request_tasks_.Enqueue(std::move(task));
}

// 로비 소속 세션이다. 로그인했고 방에 있지 않은 활성 세션이면 방 전환 대기 중이어도 포함한다.
// 로비 목록 판단과 로비 요청을 처리할지의 판단에 같이 쓴다. 로그인 전 세션은 로그인 대기 목록에 따로 있으므로 로비 방송을 받지 않는다.
bool LobbyThreadManager::ShouldBeListed(const SessionState& state)
{
	return state.life == LifeState::ACTIVE && (state.mode == ModeState::LOBBY || state.mode == ModeState::MATCHING || state.mode == ModeState::ROOM_PENDING);
}

// 요청을 보낸 세션이 지금도 로비 소속일 때만 처리한다. 받은 뒤 방으로 갔거나 연결 종료가 예약됐으면 버린다.
void LobbyThreadManager::ProcessRequest(std::unique_ptr<SessionPacketTask> task)
{
	if (!task) return;
	auto* session = game_server_.GetSessionManager().FindSession(task->session_key);
	if (!session || !ShouldBeListed(session->GetState())) return;
	ProcessPacket(task->packet.data(), *session);
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

void LobbyThreadManager::ProcessPacket(char* packet, Session& session)
{
	if (!packet) return;
	switch (reinterpret_cast<PACKET_HEADER*>(packet)->type) {
	case C2S_ADD_PUBLIC_ROOM:
	case C2S_ADD_PRIVATE_ROOM: {
		// 전환 대기로 먼저 바꾼 뒤 방 생성 요청을 넣는다.
		const SessionKey session_key = session.GetSessionKey();
		if (!session.TryChangeMode(ModeState::LOBBY, ModeState::ROOM_PENDING)) {
			game_server_.SendError(session, ErrorCode::NOT_IN_LOBBY);
			break;
		}
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
	case C2S_FAST_MATCHING: {
		// 매칭 상태로 먼저 바꾼 뒤 매칭 풀에 등록한다. 매칭 중에도 로비가 담당하므로 로비 패킷은 계속 처리된다.
		// 등록은 요청 단계에서 하고 매칭은 요청 단계가 모두 끝난 뒤 돌므로, 등록 정보를 쓰는 것과 읽는 것이 겹치지 않는다.
		if (!session.TryChangeMode(ModeState::LOBBY, ModeState::MATCHING)) {
			game_server_.SendError(session, ErrorCode::NOT_IN_LOBBY);
			break;
		}
		matchmaker_.Register(session.GetSessionKey().session_index, tick_time_ms_.load());
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
