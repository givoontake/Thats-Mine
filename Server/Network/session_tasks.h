#pragma once
#include <memory>
#include <utility>
#include <vector>
#include "ExOverlapped.h"
#include "db_login_result.h"

enum class SessionTaskType { PACKET, DISCONNECT, DB_RESULT };
enum class SessionTaskProcessResult { CONTINUE, RESTORE_REMAINING, DISCARD_REMAINING };

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
