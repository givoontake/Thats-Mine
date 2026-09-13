#pragma once
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "types.h"
#include "ActiveList.h"
#include "ConcurrentTaskQueue.h"
#include "IndexRegistry.h"
#include "GameThread.h"
#include "room_lifecycle_tasks.h"
#include "game_state.h"

class GameServer;
class Session;
class GameRoom;
struct GamePhaseContext;
class TimerThread;

class GameThreadManager
{
public:
    static constexpr int THREAD_COUNT = 12;
    static constexpr int MIN_AVAILABLE_THREAD_COUNT = THREAD_COUNT / 2;

private:
    GameServer& game_server_;
    std::mutex tick_mutex_;
    std::condition_variable tick_cv_;
	TickWaitPolicy tick_wait_policy_;
	ConcurrentTaskQueue<std::unique_ptr<RoomLifecycleTask>> lifecycle_tasks_; // 방 생성 전에는 방 내부 큐를 사용할 수 없으므로 생성 요청은 여기서 처리한다. 최종 방 삭제 요청도 여기서 처리한다.
	ActiveList<RoomKey> active_room_keys_; // 활성 방만 순회하여 게임 처리 비용을 줄인다.
	IndexRegistry<RoomKey> room_index_registry_; // room_key로 고정 방 배열의 room_index를 빠르게 찾는다.
	std::atomic<bool> is_lifecycle_processing_{ false };
    std::vector<std::unique_ptr<GameThread>> thread_objects_;
    std::vector<std::thread> threads_;

    friend class GameThread;
    friend class TimerThread;

	int GetAvailableThreadCount() const;
	std::vector<GameThread*> SelectThreads();
	void ProcessTask(std::unique_ptr<RoomLifecycleTask> task);
	int CreatePublicRoom(char* packet, Session& session, SessionKey session_key);
	int CreatePrivateRoom(char* packet, Session& session, SessionKey session_key);
	RoomKey GenerateRoomKey();
	void DeleteRoom(int room_index);
	std::shared_ptr<GameRoom> FindRoomByKey(RoomKey room_key) const;

public:
    GameThreadManager(GameServer& game_server, TickWaitPolicy tick_policy);

    void Start();
	bool StartGamePhase(long long tick_time_ms);
	void Enqueue(std::unique_ptr<RoomLifecycleTask> task);
	int TryJoinRoom(SessionKey session_key, RoomKey room_key, const std::string& room_password, int matching_max_player_count = -1);
	void ProcessPacket(char* packet, Session& session);
	SP<GameRoom> GetActiveRoom(std::size_t active_room_index) const;
    void Close();
    void Join();
};
