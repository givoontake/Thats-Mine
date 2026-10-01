#pragma once
#include <cstdint>
#include "common_packets.h"
#include "settings.h"

#pragma pack(push, 1)

// 이동 상태. 위치·속도는 cm, 방향은 도 단위이고, 이동 입력은 -127~127로 줄여 담는다.
struct MOVE_STATE {
	float location[3];
	float velocity[3];
	float yaw;
	float view_pitch;
	std::int8_t input_right;
	std::int8_t input_forward;
	bool is_falling;
	bool is_aiming;
	std::uint8_t weapon;
};

struct C2S_MOVE_PACKET {
	PACKET_HEADER header;
	MOVE_STATE move_state;
};

struct S2C_MOVE_ENTRY {
	int player_id;
	std::int64_t server_time_ms; // 서버가 이동 상태를 받은 틱 시각
	MOVE_STATE move_state;
};

// 이번 틱에 중계하는 플레이어만 담는다. 실제 크기는 player_count개의 항목까지다.
struct S2C_MOVE_PACKET {
	PACKET_HEADER header;
	std::uint8_t player_count;
	S2C_MOVE_ENTRY entries[MAX_ROOM_PLAYER_COUNT];
};

// 동작과 무기 값은 클라이언트가 정하며 서버는 그대로 중계한다.
struct C2S_ACTION_PACKET {
	PACKET_HEADER header;
	std::uint8_t action;
	std::uint8_t weapon;
};

struct S2C_ACTION_PACKET {
	PACKET_HEADER header;
	int player_id;
	std::int64_t server_time_ms; // 서버가 동작을 받은 틱 시각
	std::uint8_t action;
	std::uint8_t weapon;
};

struct S2C_SPAWN_ENTRY {
	int player_id;
	float location[3]; // 캐릭터 캡슐 중심(cm)
	float yaw;
};

// 로딩을 마친 플레이어들의 시작 좌표. 실제 크기는 player_count개의 항목까지다.
struct S2C_SPAWN_PACKET {
	PACKET_HEADER header;
	std::uint8_t player_count;
	S2C_SPAWN_ENTRY entries[MAX_ROOM_PLAYER_COUNT];
};

// 게임 레벨을 불러오고 준비를 마쳤다.
struct C2S_LOAD_COMPLETE_PACKET {
	PACKET_HEADER header;
};

// 스폰이 끝났으니 로딩 화면을 닫고 플레이를 시작한다. 받는 플레이어의 스폰 패킷 바로 뒤에 보낸다.
struct S2C_PLAY_START_PACKET {
	PACKET_HEADER header;
};

#pragma pack(pop)

static_assert(sizeof(S2C_MOVE_PACKET) <= BUF_SIZE, "S2C_MOVE_PACKET must fit in one packet.");
static_assert(sizeof(S2C_SPAWN_PACKET) <= BUF_SIZE, "S2C_SPAWN_PACKET must fit in one packet.");
