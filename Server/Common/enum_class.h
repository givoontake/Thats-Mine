#pragma once
#include <cstdint>
#include "error_code.h"

enum class OPType { SEND, POOLED_SEND, LARGE_SEND, RECV, ACCEPT, DISCONNECT_CLEANUP, ACCEPT_RETRY };


// LOADING: 게임 레벨을 불러오는 중, LOADED: 로딩을 마치고 스폰을 기다리는 중
enum class RoomPlayerState { EMPTY, WAIT, READY, LOADING, LOADED, PLAY };

// 게임 중 한 플레이어의 이동 동기화 상태.
// NORMAL은 이동 패킷을 받아 비교하는 중, ROLLBACK은 롤백을 보내고 확인을 기다리는 중(이동 패킷과 동작을 버린다),
// STOPPED는 이동 패킷이 오래 오지 않아 서버가 멈춘 것으로 보고 다른 플레이어에게 멈춤을 알린 뒤다.
// SPAWNING은 스폰 패킷을 보내고 클라이언트의 스폰 알림을 기다리는 중이다(스폰 자리에 그대로 두고 이동 패킷을 버린다).
enum class NetworkState { NORMAL, ROLLBACK, STOPPED, SPAWNING };

enum class RoomState { EMPTY, INITIALIZING, WAIT, PLAY, WAITING_DELETE, DELETE_POST };

// 게임 방의 시작 단계. LOADING: 전원 로딩 완료(또는 로딩 제한 시간)를 기다리는 중, READY: 시작 대기 시간을 세는 중, STARTED: 게임 중
// 로딩 단계에는 플레이어가 움직이지 못한다.
enum class MatchPhase { LOADING, READY, STARTED };

// 받은 이동 상태의 검사 결과. ROLLBACK: 인정하지 않고 클라이언트를 서버 이동 상태로 맞춤
enum class MoveCheckResult { ACCEPTED, ROLLBACK };

// FREE: 빈 자리 또는 초기화 중(게임 스레드가 처리하지 않음), COMPLETE: 처리 가능, PROCESSING: 처리 중
enum class RoomProcessState { FREE, COMPLETE, PROCESSING };

enum class RoomExitType { LEAVE, KICK };

// DISCONNECT: 연결 종료를 예약한 쪽이 보낸, 연결이 끊긴 플레이어의 자리를 빼라는 요청
enum class RoomTaskType { NONE, JOIN, READY, KICK, START, DISCONNECT };

// 로그인 DB 스레드의 세션 작업. NEW_SESSION: 접속을 받은 세션까지 중복 로그인 확인 범위를 늘림, PACKET: 로그인 대기 소속일 때 받은 패킷,
// DISCONNECT: 연결 종료 정리 통보
enum class DBSessionTaskType { NEW_SESSION, PACKET, DISCONNECT };

// 로비 작업. LOGIN: 로그인 성공 입장 확정, ROOM_EXIT: 방 퇴장·강퇴 입장 확정, MATCHING_CANCEL: 빠른 매칭 취소 입장 확정,
// ENTER: 다른 곳으로 가지 못하고 돌아온 입장 확정, DISCONNECT: 연결 종료 정리 통보
enum class LobbyTaskType { LOGIN, ROOM_EXIT, MATCHING_CANCEL, ENTER, DISCONNECT };

// 매칭 작업. ENTER: 로비에서 온 매칭 등록 확정, RETURN: 방 입장에 실패해 돌아온 매칭 확정, PACKET: 매칭 소속일 때 받은 패킷,
// DISCONNECT: 연결 종료 정리 통보
enum class MatchingTaskType { ENTER, RETURN, PACKET, DISCONNECT };

// 세션 상태는 연결 상태와 담당 상태를 원자 변수 하나에 함께 저장하므로 1바이트로 고정한다.
// DISCONNECT_PENDING: 연결 종료가 예약되어 소속된 곳의 정리를 기다리는 중. 송수신은 더 하지 않고, 소속은 바뀌지 않는다.
// DISCONNECT_READY: 소속된 곳이 정리를 마쳐 소속이 없음이 된 상태. 연결 종료 정리 단계가 소켓을 닫고 세션을 비운다.
enum class LifeState : std::uint8_t { NONE, INITIALIZING, ACTIVE, DISCONNECT_PENDING, DISCONNECT_READY };

// 세션이 어디에 소속돼 있는지를 나타낸다. LOGIN은 로그인 DB 스레드, LOBBY는 로비, MATCHING은 매칭 스레드, ROOM은 방이 처리한다.
// *_PENDING은 다른 처리 단위로 옮기는 요청을 보내고 받는 쪽이 확정하기를 기다리는 상태다. 받는 쪽이 자기 목록에 넣은 뒤 확정한다.
// 방은 입장을 거절할 수 있으므로, 거절하면 방이 다시 로비나 매칭으로 옮기는 요청을 보낸다.
enum class ModeState : std::uint8_t { NONE, LOGIN, LOBBY_PENDING, LOBBY, MATCHING_PENDING, MATCHING, ROOM_PENDING, ROOM };
