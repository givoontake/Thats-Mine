#pragma once
#include <WinSock2.h>
#include <array>
#include <cstdint>
#include <memory>
#include "settings.h"
#include "types.h"
#include "ActivePlayerManager.h"
#include "Session.h"

// 세션 배열을 관리한다. 새 접속에 빈 세션을 빌려주고, 세션 키로 찾아 주며, 연결이 끝난 세션을 비워 다시 쓸 수 있게 한다.
// 로그인한 플레이어 목록은 세션을 비울 때 함께 정리해야 하므로 여기서 소유한다.
// 로비나 방은 알지 않는다. 새 세션을 로비에 알리는 것은 세션을 받은 GameServer가 한다.
class SessionManager
{
	std::array<Session, MAX_PLAYER_COUNT> sessions_;
	ActivePlayerManager active_players_;

	std::uint64_t GenerateSessionID();
	void TryReleaseSession(Session& session);

public:
	Session* AcquireSession(SOCKET new_socket, HANDLE iocp_handle);
	Session* FindSessionByIndex(int session_index);
	Session* FindSession(SessionKey session_key);
	bool EnqueueSessionTask(SessionKey session_key, std::unique_ptr<SessionTask> task);
	void RequestDisconnect(SessionKey session_key);
	void CompleteSessionIO(SessionKey session_key);
	void ReleaseOwnership(Session& session);
	ActivePlayerManager& GetActivePlayerManager() { return active_players_; }
};
