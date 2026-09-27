#pragma once
#include <mutex>
#include <unordered_map>
#include "types.h"

class Session;
struct DBLoginResult;

class ActivePlayerManager
{
	std::unordered_map<int, SessionKey> active_players_;
	mutable std::mutex active_players_mutex_;

public:
	bool AddPlayer(Session& session, const DBLoginResult& result);
	void RemovePlayer(int player_id, SessionKey session_key);
	void Clear();
};
