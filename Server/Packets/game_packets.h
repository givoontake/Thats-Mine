#pragma once
#include <cstdint>
#include "common_packets.h"
#include "settings.h"
#include "platform_coordinates.h"

// 히트박스 부위. 서버 판정 데이터와 클라이언트 명중 보고가 같은 값을 쓴다.
enum class HitboxPart : std::uint8_t { HEAD, TORSO, ARM, HAND, LEG, FOOT, COUNT };

// 상자 종류. 서버 후보 데이터와 상자 배치 패킷이 같은 값을 쓴다.
enum class BoxKind : std::uint8_t { TREASURE, GIFT };

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

// 스폰이 끝났으니 로딩 화면을 닫고 맵을 보여 준다. 받는 플레이어의 스폰 패킷 바로 뒤에 보낸다.
struct S2C_PLAY_START_PACKET {
	PACKET_HEADER header;
};

// 시작 대기에 들어갔다. 남은 시간이 지나면 서버가 게임 시작을 보낸다.
struct S2C_GAME_READY_PACKET {
	PACKET_HEADER header;
	std::uint32_t remaining_ms;
};

// 시작 대기가 끝나 게임을 시작한다.
struct S2C_GAME_START_PACKET {
	PACKET_HEADER header;
};

// 클라이언트가 총으로 다른 플레이어를 맞혔을 때 보낸다. 광선은 시작점(cm)·단위 방향·맞은 곳까지의 거리(cm)이고, 부위는 클라이언트가 판정한 값이다.
struct C2S_HIT_PACKET {
	PACKET_HEADER header;
	int target_player_id;
	std::uint8_t weapon;
	std::uint8_t part;
	float origin[3];
	float direction[3];
	float distance;
};

// 서버가 인정한 명중. 쏜 사람과 맞은 사람에게만 보낸다.
struct S2C_HIT_PACKET {
	PACKET_HEADER header;
	int shooter_player_id;
	int target_player_id;
	std::uint8_t part;
};

// 명중으로 바뀐 기절 게이지 값. 맞은 사람에게만 보낸다. 이후 초당 감소는 클라이언트가 계산한다.
struct S2C_STUN_GAUGE_PACKET {
	PACKET_HEADER header;
	float gauge;
};

// 플레이어가 기절했다. 방 전체에 보낸다. 끝나면 그 플레이어의 게이지는 0이다.
struct S2C_STUN_PACKET {
	PACKET_HEADER header;
	int player_id;
	std::uint32_t duration_ms;
};

// 서버가 이동을 인정하지 않아 마지막으로 인정한 위치(캡슐 중심, cm)로 되돌린다. 받은 클라이언트는 그 위치로 바로 옮기고 같은 번호로 확인을 보낸다.
struct S2C_MOVE_CORRECTION_PACKET {
	PACKET_HEADER header;
	std::uint32_t correction_id;
	float location[3];
};

// 되돌리기를 적용했다는 확인. 서버는 이 확인을 받을 때까지 그 플레이어의 이동 상태를 받지 않는다.
struct C2S_MOVE_CORRECTION_ACK_PACKET {
	PACKET_HEADER header;
	std::uint32_t correction_id;
};

// 이번 게임의 발판 배치. 발판 자리(PLATFORM_COORDINATES)마다 놓으면 1인 비트를 자리 순서대로 담는다(바이트 안에서는 낮은 비트부터).
// platform_count는 서버의 자리 수로, 클라이언트 표와 다르면 받지 않는다.
struct S2C_PLATFORM_LAYOUT_PACKET {
	PACKET_HEADER header;
	std::uint16_t platform_count;
	std::uint8_t spawned_bits[(PLATFORM_COORDINATE_COUNT + 7) / 8];
};

// 상자 하나. 종류는 BoxKind 값이고, 위치는 cm, 회전은 언리얼 회전 순서(pitch, yaw, roll, 도)다.
struct BOX_ENTRY {
	std::uint8_t kind;
	float location[3];
	float rotation[3];
};

// 이번 게임의 상자 배치. 실제 크기는 box_count개의 항목까지이고, 상자 번호는 항목 순서다.
struct S2C_BOX_LAYOUT_PACKET {
	PACKET_HEADER header;
	std::uint8_t box_count;
	BOX_ENTRY entries[MAX_BOX_COUNT];
};

#pragma pack(pop)

static_assert(sizeof(S2C_MOVE_PACKET) <= BUF_SIZE, "S2C_MOVE_PACKET must fit in one packet.");
static_assert(sizeof(S2C_SPAWN_PACKET) <= BUF_SIZE, "S2C_SPAWN_PACKET must fit in one packet.");
static_assert(sizeof(S2C_PLATFORM_LAYOUT_PACKET) <= BUF_SIZE, "S2C_PLATFORM_LAYOUT_PACKET must fit in one packet.");
static_assert(sizeof(S2C_BOX_LAYOUT_PACKET) <= BUF_SIZE, "S2C_BOX_LAYOUT_PACKET must fit in one packet.");
