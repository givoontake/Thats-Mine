#pragma once
#include <atomic>
#include <memory>
#include "ServerThread.h"
#include "lobby_state.h"

class LobbyThreadManager;
struct LobbyPhaseContext;

class LobbyThread final : public ServerThread
{
	friend class LobbyThreadManager;

	LobbyThreadManager& manager_;
	std::atomic<LobbyThreadState> state_{ LobbyThreadState::AVAILABLE };
	std::atomic<LobbyPhase> phase_{ LobbyPhase::NONE };
	std::shared_ptr<LobbyPhaseContext> phase_context_;

	void WaitLobbyPhase();

public:
	explicit LobbyThread(LobbyThreadManager& manager);
	void Run() override;
	void Close() override;
};
