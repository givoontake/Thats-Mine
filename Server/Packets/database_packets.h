#pragma once
#include "common_packets.h"
#include "settings.h"

#pragma pack(push, 1)
// 접속한 세션이 로그인 대기 목록에 들어가 로그인 요청을 받을 수 있게 되었음을 알린다.
struct S2C_LOGIN_READY_PACKET {
	PACKET_HEADER header;
};

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
