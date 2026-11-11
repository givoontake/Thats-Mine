#pragma once
#include "enum_class.h"
#include "types.h"

class GameServer;
struct DBLoginResult;

// 로그인 대기(LOGIN) 세션을 맡는다. 로그인 DB 스레드가 소유하고 실행하며, 로그인 대기 세션의 소속 변경과 정리는 이 스레드만 한다.
// 맡은 세션인지는 세션 상태(활성이고 소속이 LOGIN)로 판단한다.
// 흐름: 접속 → 새 세션 작업으로 중복 로그인 확인 범위를 늘림 → 로그인 요청 조회 → 성공하면 로비로 옮기는 요청
class LoginManager
{
	GameServer& game_server_;
	// 중복 로그인 확인 때 훑을 세션 배열의 끝(로그인할 수 있는 가장 큰 세션 번호 + 1). 로그인 DB 스레드 하나만 바꾸고 읽는다.
	int session_end_ = 0;

	bool HasPlayer(int player_id);

public:
	explicit LoginManager(GameServer& game_server);

	void AddSession(SessionKey session_key);
	void CleanupSession(SessionKey session_key);
	bool IsWaitingLogin(SessionKey session_key);
	void ApplyLoginResult(SessionKey session_key, const DBLoginResult& result);
};
