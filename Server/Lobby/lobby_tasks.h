#pragma once
#include <utility>
#include "error_code.h"
#include "types.h"
#include "db_login_result.h"

// SESSION_ADDED: 새 접속 세션을 로그인 대기 목록에 넣고 로그인 준비 완료를 알린다. 입출력 스레드의 부담을 줄이려고 로비 사전 작업에서 처리한다.
// LOGIN_RESULT: 로그인 DB 스레드가 보낸 성공 결과를 반영한다. 중복 로그인이 아니면 로그인 목록에서 로비 목록으로 옮긴다.
// RETURN_TO_LOBBY: 방 퇴장·강퇴·입장 거절·방 생성 실패로 로비로 돌아오는 세션(LOBBY_PENDING)을 로비 상태로 확정한다.
enum class LobbyTaskType { SESSION_ADDED, LOGIN_RESULT, RETURN_TO_LOBBY };

struct LobbyTask
{
	LobbyTaskType task_type;
	SessionKey session_key;
	int result = SUCCESS;
	DBLoginResult login_result;

	LobbyTask(LobbyTaskType task_type, SessionKey session_key)
		: task_type(task_type), session_key(session_key) {}

	LobbyTask(SessionKey session_key, DBLoginResult login_result)
		: task_type(LobbyTaskType::LOGIN_RESULT), session_key(session_key), login_result(std::move(login_result)) {}
};
