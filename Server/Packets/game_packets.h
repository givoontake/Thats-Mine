#pragma once
#include <cstdint>
#include "common_packets.h"
#include "settings.h"

#pragma pack(push, 1)

struct S2C_SPAWN_ENTRY {
	int player_id;
	float location[3]; // 캐릭터 캡슐 중심(cm)
	float yaw;
};

// 게임 시작 직전에 방 인원 전체의 시작 좌표를 보낸다. 실제 크기는 player_count개의 항목까지다.
struct S2C_SPAWN_PACKET {
	PACKET_HEADER header;
	std::uint8_t player_count;
	S2C_SPAWN_ENTRY entries[MAX_ROOM_PLAYER_COUNT];
};

#pragma pack(pop)

static_assert(sizeof(S2C_SPAWN_PACKET) <= BUF_SIZE, "S2C_SPAWN_PACKET must fit in one packet.");
