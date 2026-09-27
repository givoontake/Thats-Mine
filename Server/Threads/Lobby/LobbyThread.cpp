#include <immintrin.h>
#include "LobbyThread.h"
#include "LobbyPhaseContext.h"
#include "LobbyThreadManager.h"
#include "GameServer.h"
#include "SessionManager.h"
#include "Session.h"

LobbyThread::LobbyThread(LobbyThreadManager& manager) : manager_(manager)
{
}

void LobbyThread::Run()
{
	while (is_running_.load() && manager_.game_server_.IsRunning()) {
		WaitLobbyPhase();
		if (!is_running_.load() || !manager_.game_server_.IsRunning()) break;

		auto current_phase_context = phase_context_;
		while (current_phase_context->next_lifecycle_task_index.fetch_add(1) < current_phase_context->lifecycle_task_count)
			manager_.ProcessTask(manager_.lifecycle_tasks_.Dequeue());
		// 사전 작업을 마지막으로 끝낸 스레드 하나가 매칭 단계를 실행하고, 나머지는 매칭 단계가 끝날 때까지 기다린다.
		// 매칭 단계는 세션 처리와 겹치지 않으므로 로비의 일부로서 매칭 중인 세션을 방으로 넘길 수 있다.
		const int completed_lifecycle_thread_count = current_phase_context->completed_lifecycle_thread_count.fetch_add(1) + 1;
		if (completed_lifecycle_thread_count == current_phase_context->thread_count) {
			manager_.matchmaker_.RunCycle(current_phase_context->tick_time_ms);
			current_phase_context->is_matching_complete.store(true);
		}
		while (is_running_.load() && manager_.game_server_.IsRunning() && !current_phase_context->is_matching_complete.load())
			_mm_pause();

		// 로비 목록을 경계까지 순회한다. 목록에는 전환 대기 중인 세션도 있으므로, 로비가 담당하는 상태(LOGIN, LOBBY, MATCHING)인 세션만 처리한다.
		while (is_running_.load() && manager_.game_server_.IsRunning()) {
			const int lobby_index = current_phase_context->next_lobby_index.fetch_add(1);
			if (lobby_index >= manager_.lobby_list_.GetEnd()) break;
			auto* session = manager_.FindLobbySession(lobby_index);
			if (!session) continue;
			const SessionState state = session->GetState();
			if (state.mode != ModeState::LOGIN && state.mode != ModeState::LOBBY && state.mode != ModeState::MATCHING) continue;

			const SessionDrainResult result = state.life == LifeState::ACTIVE ? manager_.game_server_.DrainSessionTasks(*session, nullptr) : SessionDrainResult::CLOSED;
			if (result != SessionDrainResult::CLOSED) continue;
			// 연결이 닫힌 세션은 로비 목록에서 뺀 뒤 담당을 놓는다. 연결이 닫혀 목록에 있으면 안 되는 상태이므로 맞추기로 빠진다.
			const int session_index = session->GetSessionKey().session_index;
			manager_.SyncLobbyList(session_index);
			manager_.game_server_.GetSessionManager().ReleaseOwnership(*session);
		}

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
