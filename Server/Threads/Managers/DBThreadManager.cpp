#include "DBThreadManager.h"
#include "GameServer.h"

DBThreadManager::DBThreadManager(GameServer& game_server)
    : driver_(sql::mysql::get_mysql_driver_instance()), login_thread_(game_server)
{
}

void DBThreadManager::Start()
{
    login_thread_.Init(driver_);
    login_thread_.Start();
    thread_ = std::thread(&ServerThread::Run, &login_thread_);
}

void DBThreadManager::Close()
{
    login_thread_.Close();
}

void DBThreadManager::Join()
{
    thread_.join();
}
