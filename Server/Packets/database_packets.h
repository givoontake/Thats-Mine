#pragma once
#include "common_packets.h"
#include "settings.h"

#pragma pack(push, 1)
struct C2S_LOGIN_PACKET {
	PACKET_HEADER header;
	char login_id[MAX_PLAYER_ID_SIZE];
	char login_password[MAX_PLAYER_PASSWORD_SIZE];
};

struct S2C_LOGIN_PACKET {
	PACKET_HEADER header;
	int player_id;
	char nickname[MAX_PLAYER_NAME_SIZE];
};
#pragma pack(pop)
