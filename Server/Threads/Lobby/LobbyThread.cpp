#include <immintrin.h>
#include <thread>
#include "LobbyThread.h"
#include "LobbyPhaseContext.h"
#include "LobbyThreadManager.h"
#include "GameServer.h"

LobbyThread::LobbyThread(LobbyThreadManager& manager) : manager_(manager)
{
}

void LobbyThread::Run()
{
	while (is_running_.load() && manager_.game_server_.IsRunning()) {
		WaitLobbyPhase();
		if (!is_running_.load() || !manager_.game_server_.IsRunning()) break;

		auto current_phase_context = phase_context_;
		// 다른 처리 단위가 넣은 로비 작업(입장 확정, 연결 종료 정리)은 먼저 차지한 스레드 하나가 요청 처리 전에 처리한다.
		// 로비 입장 확정과 정리는 이때만 하고, 요청 처리와 겹치지 않는다. 나머지 스레드는 끝날 때까지 기다린다.
		if (!current_phase_context->is_lobby_task_claimed.exchange(true)) {
			for (std::size_t i = 0; i < current_phase_context->lobby_task_count; ++i) manager_.ProcessTask(manager_.lobby_tasks_.Dequeue());
			current_phase_context->is_lobby_task_done.store(true);
		}
		for (int spin_count = 0; !current_phase_context->is_lobby_task_done.load(); ++spin_count) {
			if (spin_count > 64) std::this_thread::yield();
			else _mm_pause();
		}
		// 단계를 시작할 때 잡아 둔 로비 요청 수만큼 번호를 하나씩 받아 가며 나눠 처리한다. 그 뒤에 들어온 요청은 다음 틱에 처리한다.
		while (current_phase_context->next_request_index.fetch_add(1) < current_phase_context->request_count)
			manager_.ProcessRequest(manager_.request_tasks_.Dequeue(), current_phase_context->tick_number);
		// 방 목록을 갱신할 틱이면 요청 처리를 먼저 끝낸 스레드 하나가 갱신한다.
		if (current_phase_context->should_refresh_room_list && !current_phase_context->is_room_list_claimed.exchange(true))
			manager_.RefreshRoomList();

		phase_context_.reset();
		phase_.store(LobbyPhase::NONE);
		state_.store(LobbyThreadState::AVAILABLE);
	}
}

void LobbyThread::WaitLobbyPhase()
{
	std::unique_lock<std::mutex> lock(manager_.lobby_mutex_);
	manager_.lobby_cv_.wait(lock, [&] {
		return !is_running_.load() || !manager_.game_server_.IsRunning() || phase_.load() != LobbyPhase::NONE;
	});
}

void LobbyThread::Close()
{
	{
		std::lock_guard<std::mutex> lock(manager_.lobby_mutex_);
		is_running_ = false;
	}
	manager_.lobby_cv_.notify_all();
}
