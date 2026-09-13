#pragma once
#include "DBThreadManager.h"
#include "GameThreadManager.h"
#include "IOThreadManager.h"
#include "LobbyThreadManager.h"
#include "TimerThreadManager.h"
#include "game_state.h"

class GameServer;

class ServerThreadManager
{
    GameServer& game_server_;
	IOThreadManager io_thread_manager_;
	LobbyThreadManager lobby_thread_manager_;
    GameThreadManager game_thread_manager_;
    DBThreadManager db_thread_manager_;
    TimerThreadManager timer_thread_manager_;

    friend class TimerThread;

public:
    ServerThreadManager(GameServer& game_server, TickWaitPolicy tick_policy = TickWaitPolicy::FULL_SPIN);

    void StartThreads();
    void CloseThreads();
    void JoinThreads();
	DBThreadManager& GetDBThreadManager() { return db_thread_manager_; }
	LobbyThreadManager& GetLobbyThreadManager() { return lobby_thread_manager_; }
	GameThreadManager& GetGameThreadManager() { return game_thread_manager_; }
};
