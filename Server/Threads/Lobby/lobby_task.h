#pragma once
#include "enum_class.h"
#include "types.h"

// 다른 처리 단위가 로비에 넣는 작업. 로비 단계에서 요청 처리 전에 로비 스레드 하나가 처리한다.
struct LobbyTask
{
	LobbyTaskType task_type;
	SessionKey session_key;
	int error_code = ErrorCode::SUCCESS; // ENTER: 확정한 뒤 알릴 실패 사유. SUCCESS면 알리지 않는다.
};
