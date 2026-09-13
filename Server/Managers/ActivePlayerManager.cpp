#include "ActivePlayerManager.h"
#include "Session.h"

bool ActivePlayerManager::AddPlayer(SessionKey session_key)
{
	std::lock_guard<std::mutex> lock(active_players_mutex_);
	return active_players_.emplace(session_key.player_id, session_key).second;
}

void ActivePlayerManager::RemovePlayer(int player_id, SessionKey session_key)
{
	std::lock_guard<std::mutex> lock(active_players_mutex_);
	auto it = active_players_.find(player_id);
	if (it == active_players_.end()) return;
	if (it->second.session_index != session_key.session_index ||
		it->second.player_id != session_key.player_id ||
		it->second.session_id != session_key.session_id) return;
	active_players_.erase(it);
}

SessionKey ActivePlayerManager::FindSessionKeyByID(int player_id) const
{
	std::lock_guard<std::mutex> lock(active_players_mutex_);
	auto it = active_players_.find(player_id);
	if (it == active_players_.end()) return {};
	return it->second;
}

void ActivePlayerManager::Clear()
{
	std::lock_guard<std::mutex> lock(active_players_mutex_);
	active_players_.clear();
}
