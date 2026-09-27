#pragma once
#include "DBThread.h"

class GameServer;

// 로그인 조회 전용 DB 스레드. 조회와 비밀번호 확인을 하고, 결과를 받자마자 처리한다.
// 실패는 로그인 상태 안에서 되돌리기만 하면 되므로 직접 처리하고, 성공은 로비 목록으로 옮겨야 하므로 로비 작업으로 넘긴다.
class LoginDBThread : public DBThread
{
	GameServer* game_server_ = nullptr;

protected:
	DBLoginResult ProcessTask(const DBLoginTask& task) override;
	void HandleResult(SessionKey session_key, DBLoginResult result) override;

public:
	void SetGameServer(GameServer& game_server) { game_server_ = &game_server; }
};
