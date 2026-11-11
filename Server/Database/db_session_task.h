#pragma once
#include <vector>
#include "enum_class.h"
#include "types.h"

// 로그인 DB 스레드의 세션 작업. DB를 쓰지 않으므로 DB 연결이 없어도 처리한다.
struct DBSessionTask
{
	DBSessionTaskType task_type = DBSessionTaskType::NEW_SESSION;
	SessionKey session_key;
	long long request_time_ms = 0; // PACKET: 패킷을 받은 서버 시각. 조회 요청에 넘겨 너무 오래 기다린 요청을 버리는 데 쓴다.
	std::vector<char> packet; // PACKET: 입출력 스레드가 받은 패킷
};
