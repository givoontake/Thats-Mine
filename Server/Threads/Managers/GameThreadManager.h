#pragma once
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "types.h"
#include "ConcurrentTaskQueue.h"
#include "IndexRegistry.h"
#include "GameThread.h"
#include "room_lifecycle_tasks.h"
#include "game_state.h"

class GameServer;
class Session;
class GameRoom;
struct RoomInitData;
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
	IndexRegistry<RoomKey> room_index_registry_; // room_key로 고정 방 배열의 room_index를 빠르게 찾는다.
	// 방은 활성 목록 대신 고정 방 배열을 이 경계까지 순회한다. 이전 틱이 끝나기 전에 다음 틱을 처리하므로,
	// 삭제가 다른 방의 위치를 옮기는 활성 목록을 쓰면 한 틱의 삭제가 다른 틱의 순회에 영향을 준다.
	// 고정 배열은 방이 지워져도 다른 방의 위치가 바뀌지 않는다. 경계는 사용 중인 가장 큰 방 번호 + 1이다.
	std::atomic<int> room_index_end_{ 0 };
	// 방 생성·삭제와 경계 갱신을 묶는다. 생성과 삭제가 동시에 경계를 바꾸면 사용 중인 방이 경계 밖으로 밀려날 수 있다.
	std::mutex room_index_mutex_;
	std::atomic<bool> is_lifecycle_processing_{ false };
    std::vector<std::unique_ptr<GameThread>> thread_objects_;
    std::vector<std::thread> threads_;

    friend class GameThread;
    friend class TimerThread;

	int GetAvailableThreadCount() const;
	std::vector<GameThread*> SelectThreads();
	void ProcessTask(std::unique_ptr<RoomLifecycleTask> task);
	int PublishRoom(const RoomInitData& data, Session& session, SessionKey session_key);
	int CreatePublicRoom(char* packet, Session& session, SessionKey session_key);
	int CreatePrivateRoom(char* packet, Session& session, SessionKey session_key);
	RoomKey GenerateRoomKey();
	void DeleteRoom(int room_index, RoomKey room_key);
	GameRoom* FindRoomByKey(RoomKey room_key) const;

public:
    GameThreadManager(GameServer& game_server, TickWaitPolicy tick_policy);

    void Start();
	bool StartGamePhase(long long tick_time_ms);
	void Enqueue(std::unique_ptr<RoomLifecycleTask> task);
	int TryJoinRoom(SessionKey session_key, RoomKey room_key, const char* room_password);
	int GetRoomIndexEnd() const { return room_index_end_.load(); }
    void Close();
    void Join();
};
