#pragma once
#include "enum_class.h"
#include "settings.h"
#include "types.h"

class GameServer;
struct SessionState;
struct DBLoginResult;

// 로그인 결과 반영을 맡는다. 로그인 DB 스레드가 소유하고 실행한다.
// 흐름: 접속 → 로그인 요청은 로그인 DB 스레드 큐로 → 조회 → 성공하면 로비 상태로 확정
class LoginManager
{
	GameServer& game_server_;
	// 로그인 전 활성 세션인지
	static bool IsLoginSession(const SessionState& state);

public:
	explicit LoginManager(GameServer& game_server);

	bool IsWaitingLogin(SessionKey session_key);
	void ApplyLoginResult(SessionKey session_key, const DBLoginResult& result);
};
