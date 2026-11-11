#pragma once
#include <memory>
#include <vector>
#include "types.h"

// PACKET: 방 생성 요청을 보내고 방 전환 대기 중인 세션이 보낸 패킷. 생성 처리가 받는 쪽이다.
enum class RoomLifecycleTaskType { CREATE_PUBLIC, CREATE_PRIVATE, DELETE_ROOM, PACKET };

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
