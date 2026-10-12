#pragma once
#include "common_packets.h"
#include "settings.h"
#include "types.h"

#pragma pack(push, 1)

struct C2S_ADD_PUBLIC_ROOM_PACKET {
	PACKET_HEADER header;
	char max_player_count;
	char room_name[MAX_ROOM_NAME_SIZE];
};

struct S2C_ADD_PUBLIC_ROOM_PACKET {
	PACKET_HEADER header;
	RoomKey room_key;
	char max_player_count;
	char room_name[MAX_ROOM_NAME_SIZE];
};

struct C2S_ADD_PRIVATE_ROOM_PACKET {
	PACKET_HEADER header;
	char max_player_count;
	char room_name[MAX_ROOM_NAME_SIZE];
	char room_password[MAX_ROOM_PASSWORD_SIZE];
};

struct S2C_ADD_PRIVATE_ROOM_PACKET {
	PACKET_HEADER header;
	RoomKey room_key;
	char max_player_count;
	char room_name[MAX_ROOM_NAME_SIZE];
	char room_password[MAX_ROOM_PASSWORD_SIZE];
};

struct C2S_JOIN_PUBLIC_ROOM_PACKET {
	PACKET_HEADER header;
	RoomKey room_key;
};

struct C2S_JOIN_PRIVATE_ROOM_PACKET {
	PACKET_HEADER header;
	RoomKey room_key;
	char room_password[MAX_ROOM_PASSWORD_SIZE];
};

struct S2C_ADD_PLAYER_PACKET {
	PACKET_HEADER header;
	int player_id;
	char nickname[MAX_PLAYER_NAME_SIZE];
};

struct C2S_REMOVE_PLAYER_PACKET {
	PACKET_HEADER header;
};

struct S2C_REMOVE_PLAYER_PACKET {
	PACKET_HEADER header;
	int player_id;
};

struct C2S_READY_PACKET {
	PACKET_HEADER header;
};

struct S2C_READY_PACKET {
	PACKET_HEADER header;
	int player_id;
	bool is_ready;
};

struct C2S_START_PACKET {
	PACKET_HEADER header;
};

struct S2C_MULTI_START_PACKET {
	PACKET_HEADER header;
};

// 게임이 끝나 방이 게임 전 대기 상태로 돌아갔다. 방의 모든 플레이어에게 보내고, 모두 준비가 풀린 상태다.
struct S2C_RETURN_TO_ROOM_PACKET {
	PACKET_HEADER header;
};

struct C2S_KICK_PACKET {
	PACKET_HEADER header;
	int kick_player_id;
};

struct S2C_KICK_PACKET {
	PACKET_HEADER header;
	int kick_player_id;
};

struct S2C_UPDATE_HOST_PACKET {
	PACKET_HEADER header;
	int new_host_id;
};

struct C2S_REQUEST_ROOM_LIST_PACKET {
	PACKET_HEADER header;
};

struct S2C_ROOM_INFO_PACKET {
	PACKET_HEADER header;
	RoomKey room_key;
	char room_name[MAX_ROOM_NAME_SIZE];
	char max_player_count;
	char current_player_count;
	bool is_private;
	bool is_play;
};

struct S2C_INFO_PACKET {
	PACKET_HEADER header;
	int info_code;
};

struct C2S_FAST_MATCHING_PACKET {
	PACKET_HEADER header;
	char max_player_count;
};

#pragma pack(pop)
