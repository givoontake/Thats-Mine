#include "MatchingThreadManager.h"

MatchingThreadManager::MatchingThreadManager(GameServer& game_server)
	: matching_thread_(game_server)
{
}

void MatchingThreadManager::Start()
{
	matching_thread_.Start();
	thread_ = std::thread(&ServerThread::Run, &matching_thread_);
}

void MatchingThreadManager::Close()
{
	matching_thread_.Close();
}

void MatchingThreadManager::Join()
{
	thread_.join();
}
