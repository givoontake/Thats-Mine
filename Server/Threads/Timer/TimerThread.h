#pragma once
#include "ServerThread.h"

class ServerThreadManager;

class TimerThread final : public ServerThread
{
    ServerThreadManager& manager_;

public:
    TimerThread(ServerThreadManager& manager);
    void Run() override;
    void Close() override;
};
