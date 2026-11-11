#pragma once
#include <vector>
#include "enum_class.h"
#include "types.h"

// 다른 처리 단위가 매칭 스레드에 넣는 작업. 매칭 단계에서 매칭 주기 전에 처리한다.
struct MatchingTask
{
	MatchingTaskType task_type;
	SessionKey session_key;
	std::vector<char> packet; // PACKET: 입출력 스레드가 받은 패킷
};
