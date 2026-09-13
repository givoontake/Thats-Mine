#pragma once
#include <thread>
#include "TimerThread.h"

class ServerThreadManager;

class TimerThreadManager
{
public:
    static constexpr int THREAD_COUNT = 1;

private:
    TimerThread timer_thread_;
    std::thread thread_;

public:
    TimerThreadManager(ServerThreadManager& server_thread_manager);

    void Start();
    void Close();
    void Join();
};
