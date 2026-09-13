#pragma once
#include "common_packets.h"
#include "settings.h"

#pragma pack(push, 1)

struct S2C_MESSAGE_PACKET {
	PACKET_HEADER header;
	int player_id;
	char nickname[MAX_PLAYER_NAME_SIZE];
};

struct C2S_MESSAGE_PACKET {
	PACKET_HEADER header;
};

struct C2S_REQUEST_LOBBY_PLAYER_LIST_PACKET {
	PACKET_HEADER header;
};

struct S2C_LOBBY_PLAYER_INFO_PACKET {
	PACKET_HEADER header;
	int player_id;
	char nickname[MAX_PLAYER_NAME_SIZE];
};

#pragma pack(pop)
