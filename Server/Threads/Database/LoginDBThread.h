#pragma once
#include "DBThread.h"
#include "LoginManager.h"

class GameServer;

// 로그인 처리 단위. 로그인 처리(LoginManager)를 가지고, 로그인 요청을 받아 조회와 비밀번호 확인을 한 뒤 결과를 바로 처리한다.
// 요청을 큐에서 하나씩 순서대로 처리하므로, 조회하는 동안 같은 세션의 다음 요청은 큐에서 기다렸다가 앞선 결과가 반영된 뒤 처리된다.
// 새 세션 작업, 로그인 대기 소속일 때 받은 패킷, 연결 종료 정리 통보도 받아 로그인 대기 세션의 소속 변경과 정리를 이 스레드 하나가 한다.
class LoginDBThread : public DBThread
{
	GameServer& game_server_;
	LoginManager login_manager_;

protected:
	DBLoginResult ProcessTask(const DBLoginTask& task) override;
	void HandleResult(SessionKey session_key, DBLoginResult result) override;
	void ProcessSessionTask(const DBSessionTask& task) override;

public:
	explicit LoginDBThread(GameServer& game_server);

	LoginManager& GetLoginManager() { return login_manager_; }
};
