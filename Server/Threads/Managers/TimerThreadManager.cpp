#include "TimerThreadManager.h"

TimerThreadManager::TimerThreadManager(ServerThreadManager& server_thread_manager)
    : timer_thread_(server_thread_manager)
{
}

void TimerThreadManager::Start()
{
    timer_thread_.Start();
    thread_ = std::thread(&ServerThread::Run, &timer_thread_);
}

void TimerThreadManager::Close()
{
    timer_thread_.Close();
}

void TimerThreadManager::Join()
{
    thread_.join();
}
