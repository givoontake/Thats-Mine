#pragma once
#include <string>
#include "types.h"

struct DBLoginTask
{
	SessionKey session_key;
	std::string login_id;
	std::string password;
};
