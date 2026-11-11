#pragma once
#include <thread>
#include "MatchingThread.h"

class GameServer;

class MatchingThreadManager
{
private:
	MatchingThread matching_thread_;
	std::thread thread_;

public:
	explicit MatchingThreadManager(GameServer& game_server);

	MatchingThread& GetMatchingThread() { return matching_thread_; }
	void Start();
	void Close();
	void Join();
};
