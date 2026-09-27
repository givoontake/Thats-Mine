#pragma once
#include <memory>
#include <vector>
#include "ExOverlapped.h"
#include "types.h"

enum class RoomLifecycleTaskType { CREATE_PUBLIC, CREATE_PRIVATE, DELETE_ROOM };

struct RoomLifecycleTask
{
	RoomLifecycleTaskType task_type;
	SessionKey session_key;
	std::vector<char> packet;
	int room_index = -1;
	RoomKey room_key = 0; // 삭제할 방의 키. 방 자리가 재사용됐으면 삭제하지 않는다.

	RoomLifecycleTask(RoomLifecycleTaskType task_type, SessionKey session_key)
		: task_type(task_type), session_key(session_key) {}

	RoomLifecycleTask(int room_index, RoomKey room_key)
		: task_type(RoomLifecycleTaskType::DELETE_ROOM), room_index(room_index), room_key(room_key) {}
};
