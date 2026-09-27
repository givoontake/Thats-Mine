#include "ActivePlayerManager.h"
#include "Session.h"

bool ActivePlayerManager::AddPlayer(Session& session, const DBLoginResult& result)
{
	std::lock_guard<std::mutex> lock(active_players_mutex_);
	if (active_players_.contains(result.player_id) || !session.ApplyLoginResult(result)) return false;
	active_players_[result.player_id] = session.GetSessionKey();
	return true;
}

void ActivePlayerManager::RemovePlayer(int player_id, SessionKey session_key)
{
	std::lock_guard<std::mutex> lock(active_players_mutex_);
	auto it = active_players_.find(player_id);
	if (it == active_players_.end()) return;
	if (it->second != session_key) return;
	active_players_.erase(it);
}

void ActivePlayerManager::Clear()
{
	std::lock_guard<std::mutex> lock(active_players_mutex_);
	active_players_.clear();
}
