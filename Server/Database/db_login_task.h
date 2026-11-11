#pragma once
#include <string>
#include "types.h"

// 계정 조회 요청. 로그인 DB 스레드가 로그인 대기 소속일 때 받은 로그인 패킷으로 만들어 자기 조회 큐에 넣는다.
struct DBLoginTask
{
	SessionKey session_key;
	std::string login_id;
	std::string password;
	long long request_time_ms = 0; // 요청 패킷을 받은 서버 시각. 너무 오래 기다린 요청을 버리는 데 쓴다.
};
