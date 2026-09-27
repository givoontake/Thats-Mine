#pragma once
#include "ExOverlapped.h"

// SESSION_ADDED: 새 접속 세션을 로비 목록에 맞춰 넣는다. 입출력 스레드의 부담을 줄이려고 로비 사전 작업에서 처리한다.
// RETURN_TO_LOBBY: 방 퇴장·강퇴·입장 거절·방 생성 실패로 로비로 돌아오는 세션(LOBBY_PENDING)을 로비 상태로 확정한다.
enum class LobbyTaskType { SESSION_ADDED, RETURN_TO_LOBBY };

struct LobbyTask
{
	LobbyTaskType task_type;
	SessionKey session_key;
	int result = SUCCESS;

	LobbyTask(LobbyTaskType task_type, SessionKey session_key)
		: task_type(task_type), session_key(session_key) {}
};
