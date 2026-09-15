#pragma once
#include "error_code.h"
#include "info_code.h"

enum class OPType { SEND, POOLED_SEND, RECV, ACCEPT, DB };


enum class RoomPlayerState { EMPTY, WAIT, READY, PLAY };

enum class RoomState { EMPTY, WAIT, PLAY, WAITING_DELETE, DELETE_POST };

enum class RoomProcessState { COMPLETE, PROCESSING };

enum class ActiveEntryState { EMPTY, ACTIVE, PENDING };

enum class RoomExitType { LEAVE, KICK };

enum class RoomTaskType { NONE, JOIN, REMOVE_PLAYER, READY, KICK, START };

enum class LifeState { NONE, INITIALIZING, ACTIVE, DISCONNECT_PENDING, DISCONNECTING };

enum class ModeState { NONE, LOGIN, LOBBY, ROOM };
