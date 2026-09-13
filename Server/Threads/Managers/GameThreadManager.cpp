#include <WinSock2.h>
#include <bcrypt.h>
#include <immintrin.h>
#include "GameThreadManager.h"
#include "GameRoom.h"
#include "GameServer.h"
#include "ServerThreadManager.h"
#include "GamePhaseContext.h"
#include "LobbyThreadManager.h"
#include "lobby_tasks.h"
#include "room_packets.h"

GameThreadManager::GameThreadManager(GameServer& game_server, TickWaitPolicy tick_policy)
    : game_server_(game_server), tick_wait_policy_(tick_policy), active_room_keys_(MAX_ROOM_COUNT), room_index_registry_(MAX_ROOM_COUNT)
{
}

void GameThreadManager::Start()
{
    thread_objects_.reserve(THREAD_COUNT);
    threads_.reserve(THREAD_COUNT);
    for (int i = 0; i < THREAD_COUNT; ++i) {
        thread_objects_.emplace_back(std::make_unique<GameThread>(*this));
        thread_objects_.back()->Start();
        threads_.emplace_back(&ServerThread::Run, thread_objects_.back().get());
    }
}

int GameThreadManager::GetAvailableThreadCount() const
{
    int available_thread_count = 0;
    for (const auto& game_thread : thread_objects_) {
        if (game_thread->state_.load() == GameThreadState::AVAILABLE) ++available_thread_count;
    }
    return available_thread_count;
}

std::vector<GameThread*> GameThreadManager::SelectThreads()
{
    std::vector<GameThread*> selected_game_threads;
    selected_game_threads.reserve(THREAD_COUNT);
    for (const auto& game_thread : thread_objects_) {
        GameThreadState expected_state = GameThreadState::AVAILABLE;
        if (game_thread->state_.compare_exchange_strong(expected_state, GameThreadState::PROCESSING))
            selected_game_threads.emplace_back(game_thread.get());
    }
    return selected_game_threads;
}

bool GameThreadManager::StartGamePhase(long long tick_time_ms)
{
    if (!game_server_.IsRunning()) return false;
    const std::vector<GameThread*> selected_game_threads = SelectThreads();
    if (selected_game_threads.size() < MIN_AVAILABLE_THREAD_COUNT) {
        for (GameThread* game_thread : selected_game_threads)
            game_thread->state_.store(GameThreadState::AVAILABLE);
        return false;
    }

    // 일부 게임 스레드 작업이 완료되지 않아도 다음 틱으로 넘어갈 수 있으므로 게임 처리 시작에 항상 새 GamePhaseContext를 생성한다.
	const auto phase_context = std::make_shared<GamePhaseContext>(tick_time_ms, static_cast<int>(selected_game_threads.size()));
    {
        std::lock_guard<std::mutex> lock(tick_mutex_);
        for (GameThread* game_thread : selected_game_threads) {
            game_thread->phase_context_ = phase_context;
            game_thread->phase_.store(GamePhase::ROOM_PROCESS);
        }
    }
    tick_cv_.notify_all();
    return true;
}

void GameThreadManager::Enqueue(std::unique_ptr<RoomLifecycleTask> task)
{
	lifecycle_tasks_.Enqueue(std::move(task));
}

int GameThreadManager::TryJoinRoom(SessionKey session_key, RoomKey room_key, const std::string& room_password, int matching_max_player_count)
{
	auto* session = game_server_.FindSession(session_key);
	if (!session || session->GetModeState() != ModeState::LOBBY) return ErrorCode::INVALID_REQUEST;
	auto room = FindRoomByKey(room_key);
	if (!room) return ErrorCode::ROOM_NOT_FOUND;
	if (!game_server_.GetThreadManager().GetLobbyThreadManager().BeginRoomTransition(session_key)) return ErrorCode::INVALID_REQUEST;
	RoomTask task;
	task.task_type = RoomTaskType::JOIN;
	task.session_key = session_key;
	task.room_password = room_password;
	task.matching_max_player_count = matching_max_player_count;
	if (!room->AddRoomTask(std::move(task))) {
		auto lobby_task = std::make_unique<LobbyTask>(LobbyTaskType::ROOM_TRANSITION_RESULT, session_key);
		lobby_task->result = ErrorCode::ROOM_NOT_FOUND;
		lobby_task->matching_max_player_count = matching_max_player_count;
		game_server_.EnqueueLobbyTask(std::move(lobby_task));
	}
	return SUCCESS;
}

void GameThreadManager::ProcessTask(std::unique_ptr<RoomLifecycleTask> task)
{
	if (!task) return;
	if (task->task_type == RoomLifecycleTaskType::DELETE_ROOM) {
		DeleteRoom(task->room_index);
		return;
	}
	auto* session = game_server_.FindSession(task->session_key);
	if (!session || session->GetLifeState() != LifeState::ACTIVE) {
		auto lobby_task = std::make_unique<LobbyTask>(LobbyTaskType::ROOM_TRANSITION_RESULT, task->session_key);
		lobby_task->result = ErrorCode::INVALID_REQUEST;
		game_server_.EnqueueLobbyTask(std::move(lobby_task));
		return;
	}
	while (!session->TryStartTaskProcessing()) {
		if (!game_server_.IsRunning()) return;
		_mm_pause();
	}
	if (!session->MatchesSessionKey(task->session_key) || session->GetLifeState() != LifeState::ACTIVE || session->GetModeState() != ModeState::LOBBY) {
		session->CompleteTaskProcessing();
		auto lobby_task = std::make_unique<LobbyTask>(LobbyTaskType::ROOM_TRANSITION_RESULT, task->session_key);
		lobby_task->result = ErrorCode::INVALID_REQUEST;
		game_server_.EnqueueLobbyTask(std::move(lobby_task));
		return;
	}

	int result = ErrorCode::INVALID_REQUEST;
	switch (task->task_type) {
	case RoomLifecycleTaskType::CREATE_PUBLIC:
		result = CreatePublicRoom(task->packet.data(), *session, task->session_key);
		break;
	case RoomLifecycleTaskType::CREATE_PRIVATE:
		result = CreatePrivateRoom(task->packet.data(), *session, task->session_key);
		break;
	default:
		break;
	}
	auto lobby_task = std::make_unique<LobbyTask>(LobbyTaskType::ROOM_TRANSITION_RESULT, task->session_key);
	lobby_task->result = result;
	game_server_.EnqueueLobbyTask(std::move(lobby_task));
	if (result == SUCCESS) {
		const RoomSnapshot room_snapshot = session->GetRoomSnapshot();
		auto room = game_server_.GetRoomByIndex(room_snapshot.room_index);
		if (room) room->CompleteRoomInitialization();
	}
	session->CompleteTaskProcessing();
}

int GameThreadManager::CreatePublicRoom(char* packet, Session& session, SessionKey session_key)
{
	if (!packet) return ErrorCode::INVALID_REQUEST;
	C2S_ADD_PUBLIC_ROOM_PACKET* public_p = reinterpret_cast<C2S_ADD_PUBLIC_ROOM_PACKET*>(packet);
	PublicRoomInitData data;
	constexpr size_t MIN_ROOM_NAME_LENGTH = 4;
	if (public_p->max_player_count != MAX_ROOM_PLAYER_COUNT) return ErrorCode::INVALID_REQUEST;
	data.room_name = game_server_.CharBufToString(public_p->room_name, sizeof(public_p->room_name));
	if (data.room_name.size() < MIN_ROOM_NAME_LENGTH) return ErrorCode::INVALID_REQUEST;
	data.room_key = GenerateRoomKey();
	if (data.room_key == 0) return ErrorCode::SERVER_ERROR;

	SP<GameRoom> new_room;

	for (int i = 0; i < MAX_ROOM_COUNT; ++i) {
		if (!game_server_.GetRoomByIndex(i)) {
			data.room_index = i;
			{
				new_room = std::make_shared<GameRoom>(&game_server_, data);
				if (game_server_.TryAddRoom(i, new_room)) {
					if (!room_index_registry_.Register(data.room_key, i) || !active_room_keys_.Add(data.room_key)) {
						room_index_registry_.Unregister(data.room_key, i);
						game_server_.TryRemoveRoom(i, new_room);
						return ErrorCode::SERVER_ERROR;
					}
					if (!new_room->AddHostSession(session, session_key)) {
						active_room_keys_.Remove(data.room_key);
						room_index_registry_.Unregister(data.room_key, i);
						game_server_.TryRemoveRoom(i, new_room);
						return ErrorCode::INVALID_REQUEST;
					}
					new_room->SendCreateRoom(session);
					return SUCCESS;
				}
			}
		}
	}

	return ErrorCode::SERVER_ERROR;
}

int GameThreadManager::CreatePrivateRoom(char* packet, Session& session, SessionKey session_key)
{
	if (!packet) return ErrorCode::INVALID_REQUEST;
	C2S_ADD_PRIVATE_ROOM_PACKET* private_p = reinterpret_cast<C2S_ADD_PRIVATE_ROOM_PACKET*>(packet);
	PrivateRoomInitData data;
	constexpr size_t MIN_ROOM_NAME_LENGTH = 4;
	constexpr size_t MIN_ROOM_PASSWORD_LENGTH = 4;
	if (private_p->max_player_count != MAX_ROOM_PLAYER_COUNT) return ErrorCode::INVALID_REQUEST;
	data.room_name = game_server_.CharBufToString(private_p->room_name, sizeof(private_p->room_name));
	data.room_password = game_server_.CharBufToString(private_p->room_password, sizeof(private_p->room_password));
	if (data.room_name.size() < MIN_ROOM_NAME_LENGTH || data.room_password.size() < MIN_ROOM_PASSWORD_LENGTH) return ErrorCode::INVALID_REQUEST;
	data.room_key = GenerateRoomKey();
	if (data.room_key == 0) return ErrorCode::SERVER_ERROR;

	SP<GameRoom> new_room;

	for (int i = 0; i < MAX_ROOM_COUNT; ++i) {
		if (!game_server_.GetRoomByIndex(i)) {
			data.room_index = i;
			{
				new_room = std::make_shared<GameRoom>(&game_server_, data);
				if (game_server_.TryAddRoom(i, new_room)) {
					if (!room_index_registry_.Register(data.room_key, i) || !active_room_keys_.Add(data.room_key)) {
						room_index_registry_.Unregister(data.room_key, i);
						game_server_.TryRemoveRoom(i, new_room);
						return ErrorCode::SERVER_ERROR;
					}
					if (!new_room->AddHostSession(session, session_key)) {
						active_room_keys_.Remove(data.room_key);
						room_index_registry_.Unregister(data.room_key, i);
						game_server_.TryRemoveRoom(i, new_room);
						return ErrorCode::INVALID_REQUEST;
					}
					new_room->SendCreateRoom(session);
					return SUCCESS;
				}
			}
		}
	}
	return ErrorCode::SERVER_ERROR;
}

void GameThreadManager::ProcessPacket(char* packet, Session& session)
{
	if (!packet) return;
	const RoomSnapshot room_snapshot = session.GetRoomSnapshot();
	auto room = game_server_.GetRoomByIndex(room_snapshot.room_index);
	if (!room) {
		game_server_.SendError(session, ErrorCode::INVALID_REQUEST);
		return;
	}
	room->HandlePacket(packet, session);
}

void GameThreadManager::DeleteRoom(int room_index)
{
	auto room = game_server_.GetRoomByIndex(room_index);
	if (!room) return;
	const RoomKey room_key = room->GetRoomKey();
	active_room_keys_.Remove(room_key);
	room_index_registry_.Unregister(room_key, room_index);
	game_server_.TryRemoveRoom(room_index, room);
}

std::shared_ptr<GameRoom> GameThreadManager::FindRoomByKey(RoomKey room_key) const
{
	const auto room_index = room_index_registry_.Find(room_key);
	if (!room_index) return nullptr;
	auto room = game_server_.GetRoomByIndex(*room_index);
	if (!room || room->GetRoomKey() != room_key) return nullptr;
	return room;
}

SP<GameRoom> GameThreadManager::GetActiveRoom(std::size_t active_room_index) const
{
	const auto room_key = active_room_keys_.Get(active_room_index);
	if (!room_key) return nullptr;
	return FindRoomByKey(*room_key);
}

RoomKey GameThreadManager::GenerateRoomKey()
{
	RoomKey room_key = 0;
	do {
		if (BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(&room_key), sizeof(room_key), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) return 0;
	} while (room_key == 0 || room_index_registry_.Contains(room_key));
	return room_key;
}

void GameThreadManager::Close()
{
    for (auto& thread_object : thread_objects_)
        thread_object->Close();
}

void GameThreadManager::Join()
{
    for (auto& thread : threads_)
        thread.join();
}
