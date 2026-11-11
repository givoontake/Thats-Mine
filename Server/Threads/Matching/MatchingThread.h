#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
#include "ConcurrentTaskQueue.h"
#include "Matchmaker.h"
#include "ServerThread.h"
#include "matching_task.h"

class GameServer;
class Session;

// 매칭 처리 단위. 빠른 매칭(Matchmaker)을 가지고, 틱마다 쌓인 매칭 작업을 처리한 뒤 매칭 주기를 돈다.
// 매칭 풀은 이 스레드 하나만 바꾼다.
class MatchingThread final : public ServerThread
{
	GameServer& game_server_;
	Matchmaker matchmaker_;
	// 매칭 작업 큐: 로비의 매칭 등록 요청, 방 입장에 실패해 돌아온 세션, 매칭 소속일 때 받은 패킷, 연결 종료 정리 통보를 넣는다.
	ConcurrentTaskQueue<MatchingTask> matching_tasks_;
	std::mutex wait_mutex_;
	std::condition_variable cv_;
	// 처리할 매칭 단계의 틱 시각. 타이머가 넣고 매칭 스레드가 꺼낸다. 0이면 처리할 단계가 없다.
	long long phase_tick_time_ms_ = 0;
	// 매칭 단계를 처리하는 중인지. 처리 중에 온 틱은 건너뛴다.
	std::atomic<bool> is_processing_{ false };

public:
	explicit MatchingThread(GameServer& game_server);

	void StartMatchingPhase(long long tick_time_ms);
	void EnqueueTask(MatchingTask task);
	void ReturnToMatching(Session& session);
	void Run() override;
	void Close() override;
};
