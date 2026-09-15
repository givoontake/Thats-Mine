#include "DBThreadManager.h"
#include "GameServer.h"

static_assert(DBThreadManager::GAME_THREAD_COUNT > 0);
static_assert(DBThreadManager::LOGIN_THREAD_COUNT == 1);

DBThreadManager::DBThreadManager(GameServer& game_server)
    : game_server_(game_server), driver_(sql::mysql::get_mysql_driver_instance())
{
}

void DBThreadManager::Start()
{
    login_thread_.Init(game_server_.GetIOCPHandle(), driver_);
    for (auto& db_thread : game_threads_)
        db_thread.Init(game_server_.GetIOCPHandle(), driver_);

    threads_.reserve(THREAD_COUNT);
    login_thread_.Start();
    threads_.emplace_back(&ServerThread::Run, &login_thread_);
    for (auto& db_thread : game_threads_) {
        db_thread.Start();
        threads_.emplace_back(&ServerThread::Run, &db_thread);
    }
}

void DBThreadManager::Close()
{
    login_thread_.Close();
    for (auto& db_thread : game_threads_)
        db_thread.Close();
}

void DBThreadManager::Join()
{
    for (auto& thread : threads_)
        thread.join();
}

bool DBThreadManager::Enqueue(DBLoginTask task)
{
	return login_thread_.Enqueue(std::move(task));
}
