#include "LoginDBThread.h"
#include <botan/bcrypt.h>
#include <jdbc/cppconn/prepared_statement.h>
#include <jdbc/cppconn/resultset.h>

DBLoginResult LoginDBThread::ProcessTask(const DBLoginTask& task)
{
	const char* SQL_LOGIN =
		"SELECT player_id, nickname, password_hash "
		"FROM players "
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
