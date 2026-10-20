#pragma once
#include "ActiveList.h"
#include "enum_class.h"
#include "settings.h"
#include "types.h"

class GameServer;
struct SessionState;
struct DBLoginResult;

// 로그인 전 세션(LOGIN)의 목록과 로그인 결과 반영을 맡는다. 로그인 DB 스레드가 소유하고 실행한다.
// 흐름: 접속 → 로그인 대기 목록에 넣음 → 로그인 요청은 로그인 DB 스레드 큐로 → 조회 → 성공하면 로비 상태로 확정하고 로비 목록으로 옮김
class LoginManager
{
	GameServer& game_server_;
	// 로그인 대기 목록: 로그인 전 세션의 세션 번호
	ActiveList<MAX_PLAYER_COUNT> login_list_;

	static bool ShouldBeListed(const SessionState& state);

public:
	explicit LoginManager(GameServer& game_server);

	void SyncLoginList(int session_index);
	bool IsWaitingLogin(SessionKey session_key);
	void ApplyLoginResult(SessionKey session_key, const DBLoginResult& result);
};
