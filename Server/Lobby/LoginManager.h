#pragma once
#include "ActiveList.h"
#include "enum_class.h"
#include "settings.h"
#include "types.h"

class GameServer;
class Session;
struct SessionState;
struct DBLoginResult;

// 로그인 전 세션(LOGIN, AUTHENTICATING)의 목록과 로그인 처리를 맡는다. 로비 매니저가 소유하고 로비 스레드가 실행한다.
// 로그인 전 세션의 패킷은 이곳으로만 오므로 로그인 요청 외의 패킷은 처리되지 않는다.
// 흐름: 접속 → 로그인 목록에 넣고 준비 완료 알림 → 로그인 요청 시 AUTHENTICATING으로 바꾸고 DB 조회 요청
//       → 실패하면 DB 스레드가 LOGIN으로 되돌리고 알림, 성공하면 로비 작업으로 넘어와 로그인 목록에서 로비 목록으로 옮김
class LoginManager
{
	GameServer& game_server_;
	// 로그인 목록: 로그인 전 세션의 세션 번호. 로비 스레드가 로비 목록과 별도로 순회한다.
	ActiveList<MAX_PLAYER_COUNT> login_list_;

	static bool ShouldBeListed(const SessionState& state);

public:
	explicit LoginManager(GameServer& game_server);

	static bool IsLoginMode(ModeState mode_state);
	void SyncLoginList(int session_index);
	Session* FindLoginSession(int login_index);
	int GetLoginListEnd() const { return login_list_.GetEnd(); }

	void AddSession(SessionKey session_key);
	void ProcessPacket(char* packet, Session& session);
	void ApplyLoginResult(SessionKey session_key, const DBLoginResult& result);
};
