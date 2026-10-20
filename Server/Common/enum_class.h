#pragma once
#include <cstdint>
#include "error_code.h"

enum class OPType { SEND, POOLED_SEND, LARGE_SEND, RECV, ACCEPT, DISCONNECT_CLEANUP };


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

// DISCONNECT: 연결 종료 정리 단계가 보낸, 연결이 끊긴 플레이어의 자리를 빼라는 요청
enum class RoomTaskType { NONE, JOIN, READY, KICK, START, DISCONNECT };

// 세션 상태는 연결 상태와 담당 상태를 원자 변수 하나에 함께 저장하므로 1바이트로 고정한다.
// DISCONNECTING: 연결 종료가 예약되어 정리를 기다리는 중. 송수신은 더 하지 않고, 소켓은 정리가 끝난 뒤 닫는다.
enum class LifeState : std::uint8_t { NONE, INITIALIZING, ACTIVE, DISCONNECTING };

// 세션이 어디에 소속돼 있는지를 나타낸다. LOGIN은 로그인 DB 스레드, LOBBY·MATCHING은 로비, ROOM은 방이 처리한다.
// MATCHING은 로비에 있으면서 빠른 매칭 풀에 등록된 상태다.
// ROOM_PENDING은 방 입장·생성 요청을 보내고 방이 결과를 확정하기를 기다리는 상태다. 방은 거절할 수 있으므로 결과를 받는 쪽이 확정한다.
enum class ModeState : std::uint8_t { NONE, LOGIN, LOBBY, MATCHING, ROOM_PENDING, ROOM };
