#pragma once
#include <cstdint>
#include "error_code.h"
#include "info_code.h"

enum class OPType { SEND, POOLED_SEND, RECV, ACCEPT, DB };


enum class RoomPlayerState { EMPTY, WAIT, READY, PLAY };

enum class RoomState { EMPTY, INITIALIZING, WAIT, PLAY, WAITING_DELETE, DELETE_POST };

// FREE: 빈 자리 또는 초기화 중(게임 스레드가 처리하지 않음), COMPLETE: 처리 가능, PROCESSING: 처리 중
enum class RoomProcessState { FREE, COMPLETE, PROCESSING };

enum class RoomExitType { LEAVE, KICK };

enum class RoomTaskType { NONE, JOIN, READY, KICK, START };

// 세션 상태는 연결 상태와 담당 상태를 원자 변수 하나에 함께 저장하므로 1바이트로 고정한다.
// CLOSED: 담당자가 소켓을 닫음, RELEASING: 슬롯 정리 중
enum class LifeState : std::uint8_t { NONE, INITIALIZING, ACTIVE, CLOSED, RELEASING };

// 세션을 처리하는 담당자를 나타낸다. LOGIN·LOBBY·MATCHING은 로비, ROOM은 방이 담당한다.
// MATCHING은 로비에 있으면서 빠른 매칭 풀에 등록된 상태다.
// ROOM_PENDING, LOBBY_PENDING은 전환 대기 상태이며 받는 쪽이 결과를 확정할 때까지 아무도 세션 작업을 처리하지 않는다.
enum class ModeState : std::uint8_t { NONE, LOGIN, LOBBY, MATCHING, ROOM_PENDING, LOBBY_PENDING, ROOM };
