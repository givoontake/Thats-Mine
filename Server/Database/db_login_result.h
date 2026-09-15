#pragma once
#include <string>
#include "error_code.h"

struct DBLoginResult
{
	int error_code = ErrorCode::SERVER_ERROR;
	int player_id = -1;
	std::string nickname;
};
