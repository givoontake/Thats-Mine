#include <chrono>
#include <immintrin.h>
#include "GameThread.h"
#include "GamePhaseContext.h"
#include "GameThreadManager.h"
#include "GameServer.h"
#include "GameRoom.h"

GameThread::GameThread(GameThreadManager& manager) : manager_(manager)
{
}

void GameThread::Run()
{
    while (is_running_.load() && manager_.game_server_.IsRunning())
    {
        WaitGamePhase();
        if (!is_running_.load() || !manager_.game_server_.IsRunning()) break;

        auto current_phase_context = phase_context_; // 틱이 밀려 있어도 새로운 처리를 전달받은 시점에 한 번에 처리
		bool expected = false;
		if (current_phase_context->is_lifecycle_preparing.compare_exchange_strong(expected, true)) {
			while (is_running_.load() && manager_.game_server_.IsRunning()) {
				expected = false;
				if (!manager_.is_lifecycle_processing_.compare_exchange_weak(expected, true)) {
					_mm_pause();
					continue;
				}
				current_phase_context->lifecycle_task_count = manager_.lifecycle_tasks_.ClaimTaskCount();
				current_phase_context->is_lifecycle_ready.store(true);
				break;
			}
		}
		while (is_running_.load() && manager_.game_server_.IsRunning() && !current_phase_context->is_lifecycle_ready.load())
			_mm_pause();
		if (!is_running_.load() || !manager_.game_server_.IsRunning()) break;
		while (current_phase_context->next_lifecycle_task_index.fetch_add(1) < current_phase_context->lifecycle_task_count)
			manager_.ProcessTask(manager_.lifecycle_tasks_.Dequeue());
		const int completed_lifecycle_thread_count = current_phase_context->completed_lifecycle_thread_count.fetch_add(1) + 1;
		if (completed_lifecycle_thread_count == current_phase_context->thread_count) {
			current_phase_context->is_lifecycle_complete.store(true);
			manager_.is_lifecycle_processing_.store(false);
		}
		while (is_running_.load() && manager_.game_server_.IsRunning() && !current_phase_context->is_lifecycle_complete.load())
			_mm_pause();

		// 고정 방 배열을 경계까지 순회한다. 빈 번호는 건너뛰고, 처리 상태를 점유한 워커만 방을 처리한다.
        while (true) {
			const int room_index = current_phase_context->next_room_index.fetch_add(1);
			if (room_index >= manager_.GetRoomIndexEnd()) break;
			auto* room = manager_.GetRoomByIndex(room_index);
			// 삭제 대기 방은 새로 처리하지 않아, 삭제 쪽이 처리권을 얻어 자리를 비울 수 있게 한다.
			if (!room || room->GetRoomState() == RoomState::DELETE_POST) continue;
			RoomProcessState expected_state = RoomProcessState::COMPLETE;
			if (!room->processing_state_.compare_exchange_strong(expected_state, RoomProcessState::PROCESSING)) continue;
			room->ProcessRoomTick(current_phase_context->tick_time_ms);
			room->processing_state_.store(RoomProcessState::COMPLETE);
        }

        phase_context_.reset();
        phase_.store(GamePhase::NONE);
        state_.store(GameThreadState::AVAILABLE);
    }
}

void GameThread::WaitGamePhase()
{
    if (manager_.tick_wait_policy_ == TickWaitPolicy::FULL_SLEEP) {
        std::unique_lock<std::mutex> lock(manager_.tick_mutex_);
        manager_.tick_cv_.wait(lock, [&] {
            if (!is_running_.load() || !manager_.game_server_.IsRunning()) return true; // 종료 신호가 오면 바로 빠져나오기
            return phase_.load() != GamePhase::NONE;
            });
    }

    while (is_running_.load() && manager_.game_server_.IsRunning() && phase_.load() == GamePhase::NONE)
        _mm_pause();
}

void GameThread::Close()
{
    {
        std::lock_guard<std::mutex> lock(manager_.tick_mutex_);
        is_running_ = false;
	}
	manager_.tick_cv_.notify_all();
}
