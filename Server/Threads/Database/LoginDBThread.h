#pragma once
#include "DBThread.h"

class LoginDBThread : public DBThread
{
protected:
	DBLoginResult ProcessTask(const DBLoginTask& task) override;
};
