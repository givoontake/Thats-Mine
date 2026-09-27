#pragma once
#include <memory>
#include <utility>
#include <vector>
#include "types.h"
#include "db_login_result.h"

enum class SessionTaskType { PACKET, DISCONNECT, DB_RESULT };

// 담당자가 세션 작업을 처리한 뒤의 결과
// OWNED: 계속 담당 중, HANDED_OVER: 다른 담당자로 넘어감(남은 작업은 보존), CLOSED: 연결이 닫혀 담당자가 정리해야 함
enum class SessionDrainResult { OWNED, HANDED_OVER, CLOSED };

struct SessionTask
{
	SessionTaskType task_type;
	SessionKey session_key;

	explicit SessionTask(SessionTaskType task_type) : task_type(task_type) {}
	virtual ~SessionTask() = default;
};

struct SessionPacketTask final : SessionTask
{
	std::vector<char> packet;

	SessionPacketTask(const char* packet_data, int packet_size)
		: SessionTask(SessionTaskType::PACKET), packet(packet_data, packet_data + packet_size) {}
};

struct SessionDisconnectTask final : SessionTask
{
	SessionDisconnectTask() : SessionTask(SessionTaskType::DISCONNECT) {}
};

struct SessionDBResultTask final : SessionTask
{
	DBLoginResult result;
	explicit SessionDBResultTask(DBLoginResult result)
		: SessionTask(SessionTaskType::DB_RESULT), result(std::move(result)) {}
};
