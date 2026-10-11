#include "LoginDBThread.h"
#include <botan/bcrypt.h>
#include <jdbc/cppconn/prepared_statement.h>
#include <jdbc/cppconn/resultset.h>
#include "GameServer.h"
#include "SessionManager.h"
#include "lobby_tasks.h"

void LoginDBThread::HandleResult(SessionKey session_key, DBLoginResult result)
{
	if (!game_server_) return;
	// 성공은 로그인 목록에서 로비 목록으로 옮기는 전환이므로, 로비 사전 작업에서 반영하도록 넘긴다.
	if (result.error_code == ErrorCode::SUCCESS) {
		game_server_->EnqueueLobbyTask(std::make_unique<LobbyTask>(session_key, std::move(result)));
		return;
	}
	// 실패는 AUTHENTICATING → LOGIN으로 되돌린 뒤 알린다. 먼저 되돌려야 실패를 받은 클라이언트의 재시도가 무시되지 않는다.
	// 조회하는 사이 끊겼거나 다른 연결로 재사용된 세션이면 키가 달라 바꾸지 않는다.
	auto* session = game_server_->GetSessionManager().FindSession(session_key);
	if (!session || !session->TryChangeMode(session_key, ModeState::AUTHENTICATING, ModeState::LOGIN)) return;
	game_server_->SendError(*session, result.error_code);
}

DBLoginResult LoginDBThread::ProcessTask(const DBLoginTask& task)
{
	const char* SQL_LOGIN =
		"SELECT player_id, id, password "
		"FROM test_accounts "
		"WHERE id=? "
		"LIMIT 1";
	std::unique_ptr<sql::PreparedStatement> statement(connection_->prepareStatement(SQL_LOGIN));
	statement->setString(1, task.login_id);
	std::unique_ptr<sql::ResultSet> rows(statement->executeQuery());
	DBLoginResult result;
	result.error_code = ErrorCode::LOGIN_FAILED;
	if (rows && rows->next() && Botan::check_bcrypt(task.password, rows->getString(3).asStdString())) {
		result.player_id = rows->getInt(1);
		result.nickname = rows->getString(2);
		result.error_code = ErrorCode::SUCCESS;
	}
	return result;
}
