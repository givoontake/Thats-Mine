#pragma once
#include <WinSock2.h>
#include <array>
#include <cstdint>
#include "server_settings.h"
#include "types.h"
#include "ActiveList.h"
#include "Session.h"

// 세션 배열을 관리한다. 새 접속에 빈 세션을 빌려주고, 세션 키로 찾아 주며, 연결이 끝난 세션을 비워 다시 쓸 수 있게 한다.
// 로비나 방은 알지 않는다. 연결 종료를 예약한 세션의 정리를 어디에 통보할지는 GameServer가 정한다.
class SessionManager
{
	std::array<Session, MAX_PLAYER_COUNT> sessions_;
	// 연결 종료 후보: 종료가 예약되어 정리를 기다리는 세션 번호. 세션을 해제하면 빠진다.
	ActiveList<MAX_PLAYER_COUNT> disconnect_list_;

	std::uint64_t GenerateSessionID();
	void SyncDisconnectList(int session_index);

public:
	Session* AcquireSession(SOCKET new_socket, HANDLE iocp_handle);
	Session* FindSessionByIndex(int session_index);
	Session* FindSession(SessionKey session_key);
	ModeState RequestDisconnect(SessionKey session_key);
	void CompleteSessionIO(SessionKey session_key);
	void TryReleaseSession(Session& session);
	const ActiveList<MAX_PLAYER_COUNT>& GetDisconnectList() const { return disconnect_list_; }
};
