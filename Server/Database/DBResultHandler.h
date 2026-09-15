#pragma once
#include "db_login_result.h"

class GameServer;
class Session;

class DBResultHandler
{
public:
	static void HandleLogin(GameServer& server, Session& session, const DBLoginResult& result);
};
