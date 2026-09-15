#pragma once
#include <string>
#include "ExOverlapped.h"

struct DBLoginTask
{
	SessionKey session_key;
	std::string login_id;
	std::string password;
};
