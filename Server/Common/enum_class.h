#pragma once
#include <cstdint>
#include "error_code.h"
#include "info_code.h"

enum class OPType { SEND, POOLED_SEND, LARGE_SEND, RECV, ACCEPT, DB };


// LOADING: 게임 레벨을 불러오는 중, LOADED: 로딩을 마치고 스폰을 기다리는 중
enum class RoomPlayerState { EMPTY, WAIT, READY, LOADING, LOADED, PLAY };

enum class RoomState { EMPTY, INITIALIZING, WAIT, PLAY, WAITING_DELETE, DELETE_POST };

// 게임 방의 시작 단계. LOADING: 전원 로딩 완료(또는 로딩 제한 시간)를 기다리는 중, READY: 시작 대기 시간을 세는 중, STARTED: 게임 중
// 로딩 단계에는 플레이어가 움직이지 못한다.
enum class MatchPhase { LOADING, READY, STARTED };

// 받은 이동 상태의 검사 결과. ROLLBACK: 마지막으로 인정한 위치로 되돌림, AIR_CORRECTION: 공중 높이만 어겨 서버가 계산한 공중 위치로 맞춤
enum class MoveCheckResult { ACCEPTED, ROLLBACK, AIR_CORRECTION };

// FREE: 빈 자리 또는 초기화 중(게임 스레드가 처리하지 않음), COMPLETE: 처리 가능, PROCESSING: 처리 중
enum class RoomProcessState { FREE, COMPLETE, PROCESSING };

enum class RoomExitType { LEAVE, KICK };

enum class RoomTaskType { NONE, JOIN, READY, KICK, START };

// 세션 상태는 연결 상태와 담당 상태를 원자 변수 하나에 함께 저장하므로 1바이트로 고정한다.
// CLOSED: 담당자가 소켓을 닫음, RELEASING: 슬롯 정리 중
enum class LifeState : std::uint8_t { NONE, INITIALIZING, ACTIVE, CLOSED, RELEASING };

// 세션을 처리하는 담당자를 나타낸다. LOGIN·AUTHENTICATING·LOBBY·MATCHING은 로비, ROOM은 방이 담당한다.
// AUTHENTICATING은 로그인 요청을 DB에 보내고 결과를 기다리는 상태다. 로비가 계속 처리하므로 연결 종료도 평소처럼 처리된다.
// MATCHING은 로비에 있으면서 빠른 매칭 풀에 등록된 상태다.
// ROOM_PENDING, LOBBY_PENDING은 전환 대기 상태이며 받는 쪽이 결과를 확정할 때까지 아무도 세션 작업을 처리하지 않는다.
enum class ModeState : std::uint8_t { NONE, LOGIN, AUTHENTICATING, LOBBY, MATCHING, ROOM_PENDING, LOBBY_PENDING, ROOM };
