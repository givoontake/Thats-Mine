#include "server_settings.h"
#include "SessionManager.h"
#include <bcrypt.h>

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

// 세션의 연결 종료를 예약하고 종료 후보에 넣는다. 예약한 순간의 소속을 반환하며, 이미 예약됐거나 재사용된 세션이면 아무것도 하지 않고 NONE을 반환한다.
// 처음 예약한 한 번만 후보에 넣으므로 소속 정리 통보도 그 한 번만 한다. 수신은 더 걸지 않고, 소켓 닫기와 해제는 소속이 정리를 마친 뒤 정리 단계가 한다.
ModeState SessionManager::RequestDisconnect(SessionKey session_key)
{
	auto* session = FindSession(session_key);
	if (!session) return ModeState::NONE;
	const ModeState mode = session->BeginDisconnect(session_key);
	if (mode != ModeState::NONE) SyncDisconnectList(session_key.session_index);
	return mode;
}

void SessionManager::CompleteSessionIO(SessionKey session_key)
{
	auto* session = FindSession(session_key);
	if (session) session->CompleteIO(session_key);
}

// 연결 종료 후보 등록 여부를 현재 세션 상태에 맞춘다. 종료가 예약됐거나 정리 가능인 세션만 후보에 있다.
void SessionManager::SyncDisconnectList(int session_index)
{
	disconnect_list_.Sync(session_index, [this, session_index]() {
		auto* session = FindSessionByIndex(session_index);
		if (!session) return false;
		const LifeState life_state = session->GetLifeState();
		return life_state == LifeState::DISCONNECT_PENDING || life_state == LifeState::DISCONNECT_READY;
	});
}

// 정리 단계가 소켓을 닫은 정리 가능 세션을 비운다. 남은 입출력이 있으면 비우지 않고 다음 정리 주기에 다시 시도한다.
void SessionManager::TryReleaseSession(Session& session)
{
	const SessionKey session_key = session.GetSessionKey();
	if (!session.TryRelease()) return;
	SyncDisconnectList(session_key.session_index);
}
