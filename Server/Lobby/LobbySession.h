#pragma once
#include <atomic>
#include <cstdint>
#include "ExOverlapped.h"

class LobbySession
{
	std::atomic<std::uint64_t> session_id_{ 0 };
	std::atomic<ActiveEntryState> state_{ ActiveEntryState::EMPTY };

public:
	ActiveEntryState GetState() const;
	bool MatchesSessionKey(SessionKey session_key) const;
	bool TryPrepare(SessionKey session_key);
	bool TrySetPending(SessionKey session_key);
	bool Activate(SessionKey session_key);
	bool Clear(SessionKey session_key);
};
