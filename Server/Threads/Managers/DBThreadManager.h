#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <memory>
#include <thread>
#include <vector>
#include <jdbc/mysql_driver.h>
#include "DBThread.h"
#include "LoginDBThread.h"

class GameServer;

class DBThreadManager
{
public:
    static constexpr int GAME_THREAD_COUNT = 2;
    static constexpr int LOGIN_THREAD_COUNT = 1;
    static constexpr int THREAD_COUNT = GAME_THREAD_COUNT + LOGIN_THREAD_COUNT;

private:
    GameServer& game_server_;
    sql::mysql::MySQL_Driver* driver_ = nullptr;
    LoginDBThread login_thread_;
    std::array<DBThread, GAME_THREAD_COUNT> game_threads_;
    std::vector<std::thread> threads_;

public:
    DBThreadManager(GameServer& game_server);

    bool Enqueue(DBLoginTask task);
    void Start();
    void Close();
    void Join();
};
