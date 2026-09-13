#pragma once
#include <memory>
#include <thread>
#include <vector>
#include "IOThread.h"

class GameServer;

class IOThreadManager
{
    GameServer& game_server_;
    std::vector<std::unique_ptr<IOThread>> thread_objects_;
    std::vector<std::thread> threads_;

public:
    IOThreadManager(GameServer& game_server);

    void Start(int thread_count);
    void Close();
    void Join();
};
