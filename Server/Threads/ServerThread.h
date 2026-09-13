#pragma once
#include <atomic>

class ServerThread
{
protected:
    std::atomic<bool> is_running_{ false };

public:
    virtual ~ServerThread();
    virtual void Start();
    virtual void Run() = 0;
    virtual void Close() = 0;
};
