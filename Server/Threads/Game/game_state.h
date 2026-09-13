#pragma once

enum class GameThreadState { AVAILABLE, PROCESSING };
enum class GamePhase { NONE, ROOM_PROCESS };
enum class TickWaitPolicy { FULL_SPIN, HYBRID_SPIN, FULL_SLEEP };
