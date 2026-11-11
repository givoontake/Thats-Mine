#include <thread>
#include "ServerThreadManager.h"
#include "GameServer.h"

ServerThreadManager::ServerThreadManager(GameServer& game_server, TickWaitPolicy tick_policy)
	: game_server_(game_server), io_thread_manager_(game_server), lobby_thread_manager_(game_server), matching_thread_manager_(game_server), game_thread_manager_(game_server, tick_policy), db_thread_manager_(game_server), timer_thread_manager_(*this)
{
}

void ServerThreadManager::StartThreads()
{
	const int available_io_thread_count = static_cast<int>(std::thread::hardware_concurrency()) - LobbyThreadManager::THREAD_COUNT - MatchingThreadManager::THREAD_COUNT - GameThreadManager::THREAD_COUNT - TimerThreadManager::THREAD_COUNT - DBThreadManager::THREAD_COUNT;
	const int io_thread_count = available_io_thread_count > 0 ? available_io_thread_count : 1;

	lobby_thread_manager_.Start();
	matching_thread_manager_.Start();
    game_thread_manager_.Start();
    timer_thread_manager_.Start();
    db_thread_manager_.Start();
    io_thread_manager_.Start(io_thread_count);
} 

void ServerThreadManager::CloseThreads()
{
	io_thread_manager_.Close();
	lobby_thread_manager_.Close();
	matching_thread_manager_.Close();
    game_thread_manager_.Close();
    timer_thread_manager_.Close();
    db_thread_manager_.Close();
}

void ServerThreadManager::JoinThreads()
{
	io_thread_manager_.Join();
	lobby_thread_manager_.Join();
	matching_thread_manager_.Join();
    game_thread_manager_.Join();
    timer_thread_manager_.Join();
    db_thread_manager_.Join();
}
