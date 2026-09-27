#include "SessionManager.h"
#include <bcrypt.h>
#include "session_tasks.h"

// 빈 세션 하나를 차지해 새 접속에 배정한다. 빈 세션이 없거나 아이디를 만들지 못하면 nullptr을 반환한다.
Session* SessionManager::AcquireSession(SOCKET new_socket, HANDLE iocp_handle)
{
	const std::uint64_t session_id = GenerateSessionID();
	if (session_id == 0) return nullptr;
	for (int i = 0; i < MAX_PLAYER_COUNT; ++i) {
		if (sessions_[i].InitSession(i, session_id, new_socket, iocp_handle)) return &sessions_[i];
	}
	return nullptr;
}

std::uint64_t SessionManager::GenerateSessionID()
{
	std::uint64_t session_id = 0;
	while (session_id == 0) {
		if (BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(&session_id), sizeof(session_id), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) return 0;
	}
	return session_id;
}

Session* SessionManager::FindSessionByIndex(int session_index)
{
	if (session_index < 0 || session_index >= MAX_PLAYER_COUNT) return nullptr;
	auto* session = &sessions_[session_index];
	const LifeState life_state = session->GetLifeState();
	if (life_state == LifeState::NONE || life_state == LifeState::INITIALIZING) return nullptr;
	return session;
}

Session* SessionManager::FindSession(SessionKey session_key)
{
	auto* session = FindSessionByIndex(session_key.session_index);
	if (!session || !session->MatchesSessionKey(session_key)) return nullptr;
	return session;
}

bool SessionManager::EnqueueSessionTask(SessionKey session_key, std::unique_ptr<SessionTask> task)
{
	if (!task) return false;
	auto* session = FindSession(session_key);
	if (!session) return false;
	task->session_key = session_key;
	return session->TryEnqueueTask(session_key, std::move(task));
}

void SessionManager::RequestDisconnect(SessionKey session_key)
{
	EnqueueSessionTask(session_key, std::make_unique<SessionDisconnectTask>());
}

void SessionManager::CompleteSessionIO(SessionKey session_key)
{
	auto* session = FindSession(session_key);
	if (session && session->CompleteIO(session_key)) TryReleaseSession(*session);
}

// 연결이 닫힌 세션을, 세션이 속한 로비나 방이 자기 목록에서 뺀 뒤 호출한다. 모드를 비우고 세션 정리를 시도한다.
void SessionManager::ReleaseOwnership(Session& session)
{
	if (session.GetLifeState() != LifeState::CLOSED) return;
	session.ReleaseMode();
	TryReleaseSession(session);
}

// 로비·방의 목록 정리와 마지막 입출력 완료 중 늦게 끝난 쪽이 세션을 비운다.
void SessionManager::TryReleaseSession(Session& session)
{
	if (!session.TryBeginRelease()) return;
	const SessionKey session_key = session.GetSessionKey();
	active_players_.RemovePlayer(session_key.player_id, session_key);
	session.DiscardTasks();
	session.CompleteRelease();
}
