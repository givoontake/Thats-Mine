#include <WinSock2.h>
#include <bcrypt.h>
#include "GameThreadManager.h"
#include "GameRoom.h"
#include "GameServer.h"
#include "ServerThreadManager.h"
#include "GamePhaseContext.h"
#include "LobbyThreadManager.h"
#include "lobby_tasks.h"
#include "room_packets.h"

GameThreadManager::GameThreadManager(GameServer& game_server, TickWaitPolicy tick_policy)
    : game_server_(game_server), tick_wait_policy_(tick_policy), room_index_registry_(MAX_ROOM_COUNT)
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

// 로비가 담당자로서 호출한다. 전환 대기로 먼저 바꾼 뒤 방에 입장 요청을 넣는다.
// 요청을 넣지 못하면(방이 삭제 중) 로비에 실패 결과를 통지해 다른 입장 실패와 같은 경로로 로비 상태를 확정한다.
// room_password는 패킷의 고정 길이 비밀번호 배열이며, 공개방 입장이면 nullptr이다.
int GameThreadManager::TryJoinRoom(SessionKey session_key, RoomKey room_key, const char* room_password)
{
	auto* session = game_server_.FindSession(session_key);
	if (!session || session->GetModeState() != ModeState::LOBBY) return ErrorCode::INVALID_REQUEST;
	auto* room = FindRoomByKey(room_key);
	if (!room) return ErrorCode::ROOM_NOT_FOUND;
	if (!session->TryChangeMode(ModeState::LOBBY, ModeState::ROOM_PENDING)) return ErrorCode::INVALID_REQUEST;
	RoomTask task;
	task.task_type = RoomTaskType::JOIN;
	task.session_key = session_key;
	task.room_key = room_key;
	game_server_.CopyCharBuf(task.room_password, sizeof(task.room_password), room_password, MAX_ROOM_PASSWORD_SIZE);
	if (!room->AddRoomTask(std::move(task))) {
		session->TryChangeMode(ModeState::ROOM_PENDING, ModeState::LOBBY_PENDING);
		auto lobby_task = std::make_unique<LobbyTask>(LobbyTaskType::RETURN_TO_LOBBY, session_key);
		lobby_task->result = ErrorCode::ROOM_NOT_FOUND;
		game_server_.EnqueueLobbyTask(std::move(lobby_task));
	}
	return SUCCESS;
}

// 방 생성 요청의 받는 쪽이다. 성공하면 ROOM, 실패하면 로비로 돌려보내며 모든 분기가 결과로 끝난다.
void GameThreadManager::ProcessTask(std::unique_ptr<RoomLifecycleTask> task)
{
	if (!task) return;
	if (task->task_type == RoomLifecycleTaskType::DELETE_ROOM) {
		DeleteRoom(task->room_index, task->room_key);
		return;
	}
	// 한 세션에 걸린 전환 요청은 하나뿐이므로, 전환 대기가 아니면 이 요청의 대상이 아니다.
	auto* session = game_server_.FindSession(task->session_key);
	if (!session || session->GetModeState() != ModeState::ROOM_PENDING) return;

	int result = ErrorCode::INVALID_REQUEST;
	if (session->GetLifeState() == LifeState::ACTIVE) {
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
	}
	if (result == SUCCESS) return;

	session->TryChangeMode(ModeState::ROOM_PENDING, ModeState::LOBBY_PENDING);
	auto lobby_task = std::make_unique<LobbyTask>(LobbyTaskType::RETURN_TO_LOBBY, task->session_key);
	lobby_task->result = result;
	game_server_.EnqueueLobbyTask(std::move(lobby_task));
}

// 가장 앞의 빈 방 자리를 차지해 방을 만들고 방장을 추가한 뒤 입장을 확정한다. 실패하면 자리를 되돌린다.
// 방이 배열 앞쪽부터 채워지므로 순회 경계가 동시에 존재하는 방 수에 가깝게 유지된다.
int GameThreadManager::PublishRoom(const RoomInitData& data, Session& session, SessionKey session_key)
{
	GameRoom* new_room = nullptr;
	{
		std::lock_guard<std::mutex> lock(room_index_mutex_);
		for (int room_index = 0; room_index < MAX_ROOM_COUNT; ++room_index) {
			auto* room = game_server_.GetRoomByIndex(room_index);
			if (!room->TryClaim()) continue;
			new_room = room;
			break;
		}
		if (!new_room) return ErrorCode::SERVER_ERROR;

		const int room_index = new_room->GetRoomIndex();
		new_room->Initialize(data);
		if (!room_index_registry_.Register(data.room_key, room_index)) {
			new_room->Reset();
			return ErrorCode::SERVER_ERROR;
		}
		if (!new_room->AddHostSession(session_key) || !session.TryChangeMode(ModeState::ROOM_PENDING, ModeState::ROOM)) {
			room_index_registry_.Unregister(data.room_key, room_index);
			new_room->Reset();
			return ErrorCode::INVALID_REQUEST;
		}
		if (room_index >= room_index_end_.load()) room_index_end_.store(room_index + 1);
	}
	// 초기화가 끝나기 전까지 방 처리 상태는 FREE이므로 순회 경계 안에 있어도 처리되지 않는다.
	new_room->SendCreateRoom(session);
	new_room->CompleteRoomInitialization();
	// ROOM이 되어 로비 목록에 있으면 안 되므로 목록을 맞춘다.
	game_server_.SyncLobbyList(session_key.session_index);
	return SUCCESS;
}

int GameThreadManager::CreatePublicRoom(char* packet, Session& session, SessionKey session_key)
{
	if (!packet) return ErrorCode::INVALID_REQUEST;
	C2S_ADD_PUBLIC_ROOM_PACKET* public_p = reinterpret_cast<C2S_ADD_PUBLIC_ROOM_PACKET*>(packet);
	RoomInitData data;
	constexpr size_t MIN_ROOM_NAME_LENGTH = 4;
	if (public_p->max_player_count != MAX_ROOM_PLAYER_COUNT) return ErrorCode::INVALID_REQUEST;
	game_server_.CopyCharBuf(data.room_name, sizeof(data.room_name), public_p->room_name, sizeof(public_p->room_name));
	if (strnlen(data.room_name, sizeof(data.room_name)) < MIN_ROOM_NAME_LENGTH) return ErrorCode::INVALID_REQUEST;
	data.room_key = GenerateRoomKey();
	if (data.room_key == 0) return ErrorCode::SERVER_ERROR;

	return PublishRoom(data, session, session_key);
}

int GameThreadManager::CreatePrivateRoom(char* packet, Session& session, SessionKey session_key)
{
	if (!packet) return ErrorCode::INVALID_REQUEST;
	C2S_ADD_PRIVATE_ROOM_PACKET* private_p = reinterpret_cast<C2S_ADD_PRIVATE_ROOM_PACKET*>(packet);
	RoomInitData data;
	constexpr size_t MIN_ROOM_NAME_LENGTH = 4;
	constexpr size_t MIN_ROOM_PASSWORD_LENGTH = 4;
	if (private_p->max_player_count != MAX_ROOM_PLAYER_COUNT) return ErrorCode::INVALID_REQUEST;
	game_server_.CopyCharBuf(data.room_name, sizeof(data.room_name), private_p->room_name, sizeof(private_p->room_name));
	game_server_.CopyCharBuf(data.room_password, sizeof(data.room_password), private_p->room_password, sizeof(private_p->room_password));
	if (strnlen(data.room_name, sizeof(data.room_name)) < MIN_ROOM_NAME_LENGTH || strnlen(data.room_password, sizeof(data.room_password)) < MIN_ROOM_PASSWORD_LENGTH) return ErrorCode::INVALID_REQUEST;
	data.room_key = GenerateRoomKey();
	if (data.room_key == 0) return ErrorCode::SERVER_ERROR;

	return PublishRoom(data, session, session_key);
}

// 삭제 대기(DELETE_POST)인 방 자리를 비운다. 다른 방의 위치는 바뀌지 않으며, 마지막 방이 지워지면 뒤쪽의 연속된 빈 자리만큼 순회 경계를 당긴다.
// 이전 틱이 그 방을 처리하는 중이면 처리권을 얻지 못하므로 삭제 작업을 다시 넣어 다음 생명주기 작업에서 시도한다.
// 게임 스레드는 DELETE_POST인 방을 새로 처리하지 않으므로, 진행 중이던 처리가 끝나면 다음 시도에서 성공한다.
void GameThreadManager::DeleteRoom(int room_index, RoomKey room_key)
{
	std::lock_guard<std::mutex> lock(room_index_mutex_);
	auto* room = game_server_.GetRoomByIndex(room_index);
	if (!room || room->GetRoomKey() != room_key || room->GetRoomState() != RoomState::DELETE_POST) return;
	if (!room->TryBeginReset()) {
		Enqueue(std::make_unique<RoomLifecycleTask>(room_index, room_key));
		return;
	}
	room_index_registry_.Unregister(room_key, room_index);
	room->Reset();

	int room_index_end = room_index_end_.load();
	if (room_index != room_index_end - 1) return;
	while (room_index_end > 0 && game_server_.GetRoomByIndex(room_index_end - 1)->GetRoomState() == RoomState::EMPTY) --room_index_end;
	room_index_end_.store(room_index_end);
}

GameRoom* GameThreadManager::FindRoomByKey(RoomKey room_key) const
{
	const auto room_index = room_index_registry_.Find(room_key);
	if (!room_index) return nullptr;
	auto* room = game_server_.GetRoomByIndex(*room_index);
	if (!room || room->GetRoomKey() != room_key) return nullptr;
	return room;
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
