#pragma once
#include <string>
#include "types.h"

struct DBLoginTask
{
	SessionKey session_key;
	std::string login_id;
	std::string password;
	long long request_time_ms = 0; // 요청 패킷을 받은 서버 시각. 너무 오래 기다린 요청을 버리는 데 쓴다.
};
