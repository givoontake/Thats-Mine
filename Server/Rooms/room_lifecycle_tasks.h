#pragma once
#include <memory>
#include <vector>
#include "ExOverlapped.h"

enum class RoomLifecycleTaskType { CREATE_PUBLIC, CREATE_PRIVATE, DELETE_ROOM };

struct RoomLifecycleTask
{
	RoomLifecycleTaskType task_type;
	SessionKey session_key;
	std::vector<char> packet;
	int room_index = -1;

	RoomLifecycleTask(RoomLifecycleTaskType task_type, SessionKey session_key)
		: task_type(task_type), session_key(session_key) {}

	explicit RoomLifecycleTask(int room_index)
		: task_type(RoomLifecycleTaskType::DELETE_ROOM), room_index(room_index) {}
};
