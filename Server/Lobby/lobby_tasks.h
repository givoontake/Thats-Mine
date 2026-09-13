#pragma once
#include "ExOverlapped.h"

enum class LobbyTaskType { ADD_SESSION, ROOM_TRANSITION_RESULT, ENTER_LOBBY, DISCONNECT };

struct LobbyTask
{
	LobbyTaskType task_type;
	SessionKey session_key;
	int room_index = -1;
	int result = SUCCESS;
	int matching_max_player_count = -1;

	LobbyTask(LobbyTaskType task_type, SessionKey session_key)
		: task_type(task_type), session_key(session_key) {}
};
