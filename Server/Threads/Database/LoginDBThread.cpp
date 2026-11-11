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

// 성공은 로그인 처리가 로비로 옮기는 요청을 보낸다. 실패(계정 불일치, 시간 초과, 조회 실패)는 상태를 그대로 두어 다시 로그인할 수 있게 하고 알린다.
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

void LoginDBThread::ProcessSessionTask(const DBSessionTask& task)
{
	switch (task.task_type) {
	case DBSessionTaskType::NEW_SESSION:
		login_manager_.AddSession(task.session_key);
		break;
	// 로그인 대기 소속일 때 받은 패킷이다. 로그인 요청이면 조회 요청으로 넣고, 나머지는 버린다.
	// 받은 순서대로 작업이 오므로 같은 세션의 로그인 요청도 받은 순서대로 조회된다.
	case DBSessionTaskType::PACKET: {
		if (reinterpret_cast<const PACKET_HEADER*>(task.packet.data())->type != C2S_LOGIN) break;
		const auto* request = reinterpret_cast<const C2S_LOGIN_PACKET*>(task.packet.data());
		task_queue_.push({ task.session_key, CharBufToString(request->login_id, sizeof(request->login_id)), CharBufToString(request->login_password, sizeof(request->login_password)), task.request_time_ms });
		break;
	}
	case DBSessionTaskType::DISCONNECT:
		login_manager_.CleanupSession(task.session_key);
		break;
	default:
		break;
	}
}

DBLoginResult LoginDBThread::ProcessTask(const DBLoginTask& task)
{
	// 앞선 요청으로 이미 로그인했거나 연결이 끝난 세션의 요청은 조회하지 않는다.
	if (!login_manager_.IsWaitingLogin(task.session_key)) return {};
	const char* SQL_LOGIN =
		"SELECT account_id, login_id, password "
		"FROM account.accounts "
		"WHERE login_id=? "
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
