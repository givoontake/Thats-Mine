#include "ServerThread.h"

ServerThread::~ServerThread() = default;

void ServerThread::Start()
{
    is_running_ = true;
}
