#pragma once
#include <thread>
#include <jdbc/mysql_driver.h>
#include "LoginDBThread.h"

class GameServer;

class DBThreadManager
{
private:
    sql::mysql::MySQL_Driver* driver_ = nullptr;
    LoginDBThread login_thread_;
    std::thread thread_;

public:
    DBThreadManager(GameServer& game_server);

    LoginDBThread& GetLoginThread() { return login_thread_; }
    void Start();
    void Close();
    void Join();
};
