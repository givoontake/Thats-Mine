#pragma once
#include <WinSock2.h>
#include <array>
#include <cstdint>
#include "settings.h"
#include "types.h"
#include "ActiveList.h"
#include "ActivePlayerManager.h"
#include "Session.h"

// 세션 배열을 관리한다. 새 접속에 빈 세션을 빌려주고, 세션 키로 찾아 주며, 연결이 끝난 세션을 비워 다시 쓸 수 있게 한다.
// 로그인한 플레이어 목록은 세션을 비울 때 함께 정리해야 하므로 여기서 소유한다.
// 로비나 방은 알지 않는다. 연결 종료 후보를 어디에 정리 요청할지는 정리 단계를 실행하는 GameServer가 정한다.
class SessionManager
{
	std::array<Session, MAX_PLAYER_COUNT> sessions_;
	ActivePlayerManager active_players_;
	// 연결 종료 후보: 종료가 예약되어 정리를 기다리는 세션 번호. 세션을 해제하면 빠진다.
	ActiveList<MAX_PLAYER_COUNT> disconnect_list_;

	std::uint64_t GenerateSessionID();
	void SyncDisconnectList(int session_index);

public:
	Session* AcquireSession(SOCKET new_socket, HANDLE iocp_handle);
	Session* FindSessionByIndex(int session_index);
	Session* FindSession(SessionKey session_key);
	bool RequestDisconnect(SessionKey session_key);
	void CompleteSessionIO(SessionKey session_key);
	void TryReleaseSession(Session& session);
	const ActiveList<MAX_PLAYER_COUNT>& GetDisconnectList() const { return disconnect_list_; }
	ActivePlayerManager& GetActivePlayerManager() { return active_players_; }
};
