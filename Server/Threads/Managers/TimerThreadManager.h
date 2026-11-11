#pragma once
#include <thread>
#include "TimerThread.h"

class ServerThreadManager;

class TimerThreadManager
{
private:
    TimerThread timer_thread_;
    std::thread thread_;

public:
    TimerThreadManager(ServerThreadManager& server_thread_manager);

    void Start();
    void Close();
    void Join();
};
