#pragma once
#include <cstdint>
#include "common_packets.h"
#include "settings.h"
#include "platform_coordinates.h"

// 히트박스 부위. 서버 판정 데이터와 클라이언트 명중 보고가 같은 값을 쓴다.
enum class HitboxPart : std::uint8_t { HEAD, TORSO, ARM, HAND, LEG, FOOT, COUNT };

// 상자 종류. 서버 후보 데이터와 상자 배치 패킷이 같은 값을 쓴다.
enum class BoxKind : std::uint8_t { TREASURE, GIFT };

// 탑 종류. 값은 0부터 TOWER_KIND_COUNT - 1까지이고, 서버 충돌 배치물과 탑 배치 패킷이 같은 값을 쓴다.
enum class TowerKind : std::uint8_t { TOWER_02, TOWER_04 };

// 플레이어 동작. 클라이언트 EPlayerAction과 같은 순서다.
enum class PlayerAction : std::uint8_t { FIRE, MELEE, RELOAD, PICKUP };

// 무기. 클라이언트 EWeaponType과 같은 순서다.
enum class WeaponType : std::uint8_t { PICKAXE, SNIPER, PISTOL };

// 힌트 종류. 클라이언트는 종류마다 문장 틀을 갖고, 힌트 항목의 값으로 빈칸을 채운다.
enum class HintKind : std::uint8_t { DIRECTION, VIEW, NEAREST_OBJECT, NEAREST_TOWER };

// "보물에서 가까운 물체" 힌트가 말하는 물체 종류. 메시 여러 개를 한 종류로 묶는다.
enum class HintObjectKind : std::uint8_t {
	LARGE_ROCK,
	SMALL_ROCK,
	OAK,
	SPRUCE,
	BIRCH,
	POPLAR,
	MAPLE,
	BAMBOO,
	DAISY,
	MUSHROOM,
	COUNT,
};

// 방향 힌트의 방위. 지도 기준으로 북쪽이 x가 작은 쪽, 동쪽이 y가 작은 쪽이다.
enum class HintDirection : std::uint8_t { NORTH, EAST, SOUTH, WEST, COUNT };

// 힌트 등급. A가 가장 높다.
enum class HintGrade : std::uint8_t { S, A, B, C };

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

// 동작과 무기 값은 클라이언트가 정한다. 총 발사와 재장전은 서버가 탄약을 확인해 인정한 것만 중계한다.
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
	bool is_opened;
};

// 이번 게임의 상자 배치. 이 구조체 뒤에 box_count개의 BOX_ENTRY가 이어 붙고, 상자 번호는 항목 순서다.
// 상자 수에 따라 BUF_SIZE보다 커질 수 있어 상자 배치만 크기에 맞춘 버퍼로 만들어 보낸다.
struct S2C_BOX_LAYOUT_PACKET {
	PACKET_HEADER header;
	std::uint16_t box_count;
};

// 힌트 하나. 문장은 클라이언트가 kind의 문장 틀에 값을 채워 만들고, 영역 번호는 서버 맵 영역 파일의 순서다.
// DIRECTION: target은 영역 번호, detail은 HintDirection, is_true면 보물이 그 영역보다 그 방위에 있다.
// VIEW: target은 영역 번호, is_true면 그 영역에서 보물이 묻힌 자리가 보인다.
// NEAREST_OBJECT: target은 HintObjectKind, detail은 순위(0이 가장 가깝다), is_true는 항상 참이다.
// NEAREST_TOWER: target은 TowerKind, is_true는 항상 참이다.
// grade는 HintGrade다.
struct HINT_ENTRY {
	std::uint8_t kind;
	std::uint8_t target;
	std::uint8_t detail;
	bool is_true;
	std::uint8_t grade;
};

// 서버가 아는 무기 하나의 탄약 상태(탄창의 탄, 소지 탄). 서버가 발사나 재장전을 인정하지 않았을 때와 탄약 상자를 열었을 때 그 사람에게만 보낸다.
struct S2C_AMMO_PACKET {
	PACKET_HEADER header;
	std::uint8_t weapon;
	std::uint16_t rounds;
	std::uint16_t spare_rounds;
};

// 서버가 아는 곡괭이 사용 가능 횟수와 다음 1번이 찰 때까지 남은 시간(ms, 다 차 있으면 0). 서버가 곡괭이 휘두르기를 인정하지 않았을 때 그 사람에게만 보낸다.
struct S2C_PICKAXE_CHARGES_PACKET {
	PACKET_HEADER header;
	std::uint8_t charges;
	std::uint32_t remaining_ms;
};

// 상자를 열려고 한다. 상자 번호는 상자 배치의 항목 순서다. 쏴서 열면 is_shot이 참이고 weapon은 쏜 무기다. 선물만 쏴서 열 수 있다.
struct C2S_BOX_OPEN_PACKET {
	PACKET_HEADER header;
	std::uint16_t box;
	bool is_shot;
	std::uint8_t weapon;
};

// 서버가 인정한 상자 열림. 플레이 중인 방 전체에 보낸다.
struct S2C_BOX_OPEN_PACKET {
	PACKET_HEADER header;
	std::uint16_t box;
};

// 연 상자에 든 힌트. 연 사람에게만 상자 열림 패킷 뒤에 이어 보낸다. 탄약 상자는 힌트 대신 탄약 상태를 보낸다.
struct S2C_HINT_PACKET {
	PACKET_HEADER header;
	HINT_ENTRY hint;
};

// 탑 하나. 위치는 탑 바닥 가운데(cm)다.
struct TOWER_ENTRY {
	std::uint8_t kind;
	float location[3];
};

// 이번 게임의 탑 배치. 실제 크기는 tower_count개의 항목까지다. 기초는 클라이언트가 탑 아래에 붙인다.
struct S2C_TOWER_LAYOUT_PACKET {
	PACKET_HEADER header;
	std::uint8_t tower_count;
	TOWER_ENTRY entries[TOWER_COUNT];
};

#pragma pack(pop)

static_assert(sizeof(S2C_MOVE_PACKET) <= BUF_SIZE, "S2C_MOVE_PACKET must fit in one packet.");
static_assert(sizeof(S2C_SPAWN_PACKET) <= BUF_SIZE, "S2C_SPAWN_PACKET must fit in one packet.");
static_assert(sizeof(S2C_PLATFORM_LAYOUT_PACKET) <= BUF_SIZE, "S2C_PLATFORM_LAYOUT_PACKET must fit in one packet.");
static_assert(sizeof(S2C_TOWER_LAYOUT_PACKET) <= BUF_SIZE, "S2C_TOWER_LAYOUT_PACKET must fit in one packet.");
