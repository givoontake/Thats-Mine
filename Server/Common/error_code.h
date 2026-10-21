#pragma once

enum ErrorCode
{
	SUCCESS = 0,
	SERVER_ERROR = 1,
	INVALID_REQUEST = 2,
	LOGIN_FAILED = 3,
	DUPLICATE_LOGIN_ID = 4,
	LOGIN_UNAVAILABLE = 5, // 로그인 DB 스레드가 멈춰 요청을 받지 못함
	LOGIN_TIMEOUT = 6, // DB 연결이 끊긴 사이 요청이 너무 오래 기다림
	LOGIN_QUERY_FAILED = 7, // 계정 조회 쿼리가 실패함

	ROOM_NOT_FOUND = 10,
	ROOM_FULL = 11,
	ROOM_IN_GAME = 12,
	NOT_FOUND_JOINABLE_ROOM = 13,
	ROOM_ALREADY_JOINED = 14,
	ROOM_LIMIT_REACHED = 15, // 만들 수 있는 방 수를 모두 씀
	ROOM_CREATE_FAILED = 16, // 방 키를 만들거나 등록하지 못함
	ROOM_NAME_TOO_SHORT = 17,
	ROOM_PASSWORD_TOO_SHORT = 18,

	ROOM_NOT_ALL_READY = 20,
	ROOM_NOT_ENOUGH_PLAYERS = 21,
	ROOM_INVALID_PASSWORD = 22,

	PLAYER_NOT_FOUND = 30,

	NOT_IN_LOBBY = 40, // 로비에 있을 때만 할 수 있는 요청을 다른 상태에서 보냄(방 생성·입장·빠른 매칭)
};
