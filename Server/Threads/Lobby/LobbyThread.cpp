#include <immintrin.h>
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

		// 단계를 시작할 때 잡아 둔 로비 요청 수만큼 번호를 하나씩 받아 가며 나눠 처리한다. 그 뒤에 들어온 요청은 다음 틱에 처리한다.
		auto current_phase_context = phase_context_;
		while (current_phase_context->next_request_index.fetch_add(1) < current_phase_context->request_count)
			manager_.ProcessRequest(manager_.request_tasks_.Dequeue());
		// 요청 처리를 마지막으로 끝낸 스레드 하나가 매칭을 실행한다. 매칭은 요청 처리가 모두 끝난 뒤에 돌아 요청 처리와 겹치지 않는다.
		// 매칭을 맡지 않은 스레드는 방 목록을 갱신할 틱이면 갱신한다. 그런 스레드가 여럿이면 먼저 차지한 하나만 갱신한다.
		const int completed_request_thread_count = current_phase_context->completed_request_thread_count.fetch_add(1) + 1;
		if (completed_request_thread_count == current_phase_context->thread_count)
			manager_.matchmaker_.RunCycle(current_phase_context->tick_time_ms);
		else if (current_phase_context->should_refresh_room_list && !current_phase_context->is_room_list_claimed.exchange(true))
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
