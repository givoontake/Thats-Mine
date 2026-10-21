#pragma once
#include "DBThread.h"
#include "LoginManager.h"

class GameServer;

// 로그인 처리 단위. 로그인 대기 목록과 로그인 처리(LoginManager)를 가지고, 로그인 요청을 받아 조회와 비밀번호 확인을 한 뒤 결과를 바로 처리한다.
// 요청을 큐에서 하나씩 순서대로 처리하므로, 조회하는 동안 같은 세션의 다음 요청은 큐에서 기다렸다가 앞선 결과가 반영된 뒤 처리된다.
class LoginDBThread : public DBThread
{
	GameServer& game_server_;
	LoginManager login_manager_;

protected:
	DBLoginResult ProcessTask(const DBLoginTask& task) override;
	void HandleResult(SessionKey session_key, DBLoginResult result) override;

public:
	explicit LoginDBThread(GameServer& game_server);

	// 입출력 스레드가 로그인 전 세션의 패킷을 넘긴다. 로그인 요청만 조회 작업으로 바꿔 큐에 넣는다.
	void EnqueueLoginPacket(SessionKey session_key, const char* packet, long long receive_time_ms);
	LoginManager& GetLoginManager() { return login_manager_; }
};
