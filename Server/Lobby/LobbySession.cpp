#include "LobbySession.h"

ActiveEntryState LobbySession::GetState() const
{
	return state_.load();
}

bool LobbySession::MatchesSessionKey(SessionKey session_key) const
{
	return session_id_.load() == session_key.session_id && session_key.session_id != 0;
}

bool LobbySession::TryPrepare(SessionKey session_key)
{
	if (session_key.session_id == 0) return false;
	ActiveEntryState expected_state = ActiveEntryState::EMPTY;
	if (!state_.compare_exchange_strong(expected_state, ActiveEntryState::PENDING)) return false;
	session_id_.store(session_key.session_id);
	return true;
}

bool LobbySession::TrySetPending(SessionKey session_key)
{
	if (!MatchesSessionKey(session_key)) return false;
	ActiveEntryState expected_state = ActiveEntryState::ACTIVE;
	return state_.compare_exchange_strong(expected_state, ActiveEntryState::PENDING);
}

bool LobbySession::Activate(SessionKey session_key)
{
	if (!MatchesSessionKey(session_key)) return false;
	ActiveEntryState expected_state = ActiveEntryState::PENDING;
	return state_.compare_exchange_strong(expected_state, ActiveEntryState::ACTIVE);
}

bool LobbySession::Clear(SessionKey session_key)
{
	if (!MatchesSessionKey(session_key)) return false;
	ActiveEntryState expected_state = state_.load();
	while (expected_state != ActiveEntryState::EMPTY) {
		if (state_.compare_exchange_weak(expected_state, ActiveEntryState::PENDING)) break;
	}
	if (expected_state == ActiveEntryState::EMPTY || !MatchesSessionKey(session_key)) return false;
	session_id_.store(0);
	state_.store(ActiveEntryState::EMPTY);
	return true;
}
