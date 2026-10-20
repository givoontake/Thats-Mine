#include "LoginDBThread.h"
#include <botan/bcrypt.h>
#include <jdbc/cppconn/prepared_statement.h>
#include <jdbc/cppconn/resultset.h>
#include "GameServer.h"
#include "SessionManager.h"
#include "char_buffer.h"
#include "database_packets.h"
#include "packet_types.h"

LoginDBThread::LoginDBThread(GameServer& game_server)
	: game_server_(game_server), login_manager_(game_server)
{
}

void LoginDBThread::EnqueueLoginPacket(SessionKey session_key, const char* packet)
{
	if (reinterpret_cast<const PACKET_HEADER*>(packet)->type != C2S_LOGIN) return;
	const auto* request = reinterpret_cast<const C2S_LOGIN_PACKET*>(packet);
	DBLoginTask task{ session_key, CharBufToString(request->login_id, sizeof(request->login_id)), CharBufToString(request->login_password, sizeof(request->login_password)) };
	// 큐가 요청을 받지 못하는 것은 DB 스레드가 멈췄을 때뿐이다. 다시 로그인할 수 있게 상태는 그대로 두고 실패를 알린다.
	if (Enqueue(std::move(task))) return;
	auto* session = game_server_.GetSessionManager().FindSession(session_key);
	if (session) game_server_.SendError(*session, ErrorCode::SERVER_ERROR);
}

// 성공은 로그인 처리가 로비 상태로 확정하고 목록을 옮긴다. 실패는 상태를 그대로 두어 다시 로그인할 수 있게 하고 알린다.
// 조회하는 사이 끊겼거나 다른 연결로 재사용됐거나 이미 로그인한 세션이면 알리지 않는다.
void LoginDBThread::HandleResult(SessionKey session_key, DBLoginResult result)
{
	if (result.error_code == ErrorCode::SUCCESS) {
		login_manager_.ApplyLoginResult(session_key, result);
		return;
	}
	if (!login_manager_.IsWaitingLogin(session_key)) return;
	auto* session = game_server_.GetSessionManager().FindSession(session_key);
	if (session) game_server_.SendError(*session, result.error_code);
}

DBLoginResult LoginDBThread::ProcessTask(const DBLoginTask& task)
{
	// 앞선 요청으로 이미 로그인했거나 연결이 끝난 세션의 요청은 조회하지 않는다.
	if (!login_manager_.IsWaitingLogin(task.session_key)) return {};
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
