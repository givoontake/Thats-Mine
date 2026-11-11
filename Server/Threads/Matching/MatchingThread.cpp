#include "MatchingThread.h"
#include "GameServer.h"
#include "Session.h"

MatchingThread::MatchingThread(GameServer& game_server)
	: game_server_(game_server), matchmaker_(game_server)
{
}

// 타이머가 틱마다 호출한다. 앞선 매칭 단계가 끝나지 않았으면 이 틱은 건너뛴다.
void MatchingThread::StartMatchingPhase(long long tick_time_ms)
{
	if (!game_server_.IsRunning() || is_processing_.load()) return;
	{
		std::lock_guard<std::mutex> lock(wait_mutex_);
		is_processing_.store(true);
		phase_tick_time_ms_ = tick_time_ms;
	}
	cv_.notify_one();
}

void MatchingThread::EnqueueTask(MatchingTask task)
{
	matching_tasks_.Enqueue(std::move(task));
}

// 전환 대기 중인 세션을 매칭으로 되돌리는 요청을 보낸다. 입장 요청을 받았지만 처리하지 못한 쪽(전환 대기의 주체)이 호출한다.
// 연결 종료가 예약돼 바꾸지 못하면 정리 가능으로 바꾼다. 전환 대기 중에는 정리 통보가 오지 않으므로 호출한 쪽이 최종 소속이다.
void MatchingThread::ReturnToMatching(Session& session)
{
	if (session.TryChangeMode(ModeState::ROOM_PENDING, ModeState::MATCHING_PENDING)) {
		EnqueueTask({ MatchingTaskType::RETURN, session.GetSessionKey() });
		return;
	}
	session.SetDisconnectReady(ModeState::ROOM_PENDING);
}

void MatchingThread::Run()
{
	while (is_running_.load() && game_server_.IsRunning()) {
		long long tick_time_ms = 0;
		{
			std::unique_lock<std::mutex> lock(wait_mutex_);
			cv_.wait(lock, [this]() { return !is_running_.load() || !game_server_.IsRunning() || phase_tick_time_ms_ != 0; });
			tick_time_ms = phase_tick_time_ms_;
			phase_tick_time_ms_ = 0;
		}
		if (!is_running_.load() || !game_server_.IsRunning()) break;

		// 단계를 시작할 때까지 쌓인 작업을 먼저 처리해 풀을 맞춘 뒤 매칭 주기를 돈다. 그 뒤에 들어온 작업은 다음 단계에 처리한다.
		const std::size_t task_count = matching_tasks_.ClaimTaskCount();
		for (std::size_t i = 0; i < task_count; ++i) matchmaker_.ProcessTask(matching_tasks_.Dequeue(), tick_time_ms);
		matchmaker_.RunCycle(tick_time_ms);
		is_processing_.store(false);
	}
}

void MatchingThread::Close()
{
	{
		std::lock_guard<std::mutex> lock(wait_mutex_);
		is_running_ = false;
	}
	cv_.notify_one();
}
