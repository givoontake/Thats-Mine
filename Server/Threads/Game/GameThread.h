#pragma once
#include <atomic>
#include <memory>
#include "ServerThread.h"
#include "game_state.h"

class GameThreadManager;
struct GamePhaseContext;

class GameThread final : public ServerThread
{
    friend class GameThreadManager;

    GameThreadManager& manager_;
    std::atomic<GameThreadState> state_{ GameThreadState::AVAILABLE };
    std::atomic<GamePhase> phase_{ GamePhase::NONE };
    std::shared_ptr<GamePhaseContext> phase_context_;

    void WaitGamePhase();

public:
    GameThread(GameThreadManager& manager);
    void Run() override;
    void Close() override;
};
