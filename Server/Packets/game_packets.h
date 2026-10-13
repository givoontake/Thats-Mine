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
enum class PlayerAction : std::uint8_t { FIRE, MELEE, RELOAD, PICKUP, SEARCH };

// 무기. 클라이언트 EWeaponType과 같은 순서다.
enum class WeaponType : std::uint8_t { PICKAXE, SNIPER, PISTOL, MAGNIFIER };

// 아이템 종류. 등급은 S·A뿐이고 HintGrade 값을 쓴다. 넓은 탐지 돋보기 앞까지는 얻으면 계속 적용되는 아이템이다.
enum class ItemKind : std::uint8_t { PICKAXE_CHARGE, STUN_GAUGE_DECAY, STUN_GAUGE_DECAY_DELAY, STUN_PROTECTION, MAGNIFIER_RADIUS, MAGNIFIER_COOLDOWN, WIDE_MAGNIFIER, COUNT };

// 탐색 요청과 결과에서 기본 돋보기를 뜻하는 등급 값. 넓은 탐지 돋보기는 HintGrade 값을 쓴다.
constexpr std::uint8_t BASIC_MAGNIFIER_GRADE = 0xFF;

// 힌트 종류. 클라이언트는 종류마다 문장 틀을 갖고, 힌트 항목의 값으로 빈칸을 채운다.
enum class HintKind : std::uint8_t { DIRECTION, VIEW, NEAREST_OBJECT, NEAREST_TOWER, PLATFORM_PATH, OBJECT_COUNT };

// 힌트가 말하는 물체 종류. 메시 여러 개를 한 종류로 묶는다.
// "보물에서 가까운 물체" 힌트는 MUSHROOM까지만 쓰고, 색 데이지·버섯은 DAISY·MUSHROOM으로 본다. 개수 힌트는 모든 종류를 쓰고, DAISY·MUSHROOM은 모든 색을 합친 수다.
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
	DAISY_RED,
	DAISY_MAGENTA,
	DAISY_BLUE,
	DAISY_YELLOW,
	DAISY_WHITE,
	MUSHROOM_RED,
	MUSHROOM_PURPLE,
	MUSHROOM_BLUE,
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

// 시작 대기가 끝나 게임을 시작한다. remaining_ms는 게임 제한시간이 끝날 때까지 남은 시간이다.
struct S2C_GAME_START_PACKET {
	PACKET_HEADER header;
	std::uint32_t remaining_ms;
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

// 플레이어가 기절했다. 방 전체에 보낸다. 끝나면 그 플레이어의 게이지는 0이고, 이어서 protection_ms 동안 보호된다.
struct S2C_STUN_PACKET {
	PACKET_HEADER header;
	int player_id;
	std::uint32_t duration_ms;
	std::uint32_t protection_ms;
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
// PLATFORM_PATH: target은 발판 길 번호이고, 이 힌트를 얻은 사람에게는 그 길의 발판이 보인다. is_true는 항상 참이다.
// OBJECT_COUNT: target은 HintObjectKind, detail은 보물에서 수평 HINT_OBJECT_COUNT_RADIUS 안에 있는 그 종류 물체 수(최대 255)다. is_true는 항상 참이다.
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

// 방향 힌트가 지도에서 배제하는 영역. boundary 선보다 direction(HintDirection) 쪽 전부가 배제된다.
// 북·남이면 boundary는 x, 동·서면 y(cm)다. 방향 힌트를 보낼 때만 힌트 패킷 뒤에 이어 보낸다.
struct S2C_HINT_EXCLUSION_PACKET {
	PACKET_HEADER header;
	std::uint8_t direction;
	float boundary;
};

// 기절한 플레이어가 힌트 하나를 떨어뜨렸다. 플레이 중인 방 전체에 보내고, 게임 도중 들어온 플레이어에게는 아직 줍지 않은 것만 보낸다.
// 드랍 번호는 이번 게임에서 떨어뜨린 순서다. 위치는 떨어뜨린 플레이어의 캡슐 중심(cm)이고, 클라이언트가 그 아래 바닥에 놓는다.
// 등급은 떨어진 힌트의 등급이며 누구나 두루마리 모습으로 알 수 있다.
struct S2C_HINT_DROP_PACKET {
	PACKET_HEADER header;
	std::uint16_t drop;
	float location[3];
	std::uint8_t grade;
};

// 기절해서 떨어뜨린 힌트. 떨어뜨린 사람에게만 드랍 패킷 뒤에 이어 보낸다. 받은 클라이언트는 같은 힌트를 인벤토리와 지도 배제에서 뺀다.
struct S2C_HINT_LOST_PACKET {
	PACKET_HEADER header;
	HINT_ENTRY hint;
};

// 떨어진 힌트를 주우려고 한다. 드랍 번호는 드랍 패킷의 번호다.
struct C2S_HINT_PICKUP_PACKET {
	PACKET_HEADER header;
	std::uint16_t drop;
};

// 서버가 인정한 줍기. 플레이 중인 방 전체에 보낸다. 주운 사람에게는 이어서 힌트를 보낸다.
struct S2C_HINT_PICKUP_PACKET {
	PACKET_HEADER header;
	std::uint16_t drop;
};

// 다음 공개 힌트까지 남은 시간. 게임 시작 때와 공개 차례가 지날 때마다 플레이 중인 방 전체에 보내고, 게임 도중 합류하면 그 사람에게 보낸다.
// 공개 힌트는 차례가 되면 힌트 패킷으로 플레이 중인 전원에게 같은 힌트를 보낸다.
struct S2C_PUBLIC_HINT_TIME_PACKET {
	PACKET_HEADER header;
	std::uint32_t remaining_ms;
};

// 보물을 발견한 플레이어와 보물 바닥 가운데 위치(cm), 보물 방향(도). 플레이 중인 방 전체에 보낸다. GAME_END_WAIT_MS 뒤 게임이 끝나고 방으로 돌아간다.
// 보물 위치는 발견되기 전에는 보내지 않는다.
struct S2C_TREASURE_FOUND_PACKET {
	PACKET_HEADER header;
	int player_id;
	float location[3];
	float yaw;
};

// 연 상자에 든 아이템. 종류는 ItemKind, 등급은 HintGrade다. 연 사람에게만 상자 열림 패킷 뒤에 이어 보낸다.
struct S2C_ITEM_PACKET {
	PACKET_HEADER header;
	std::uint8_t kind;
	std::uint8_t grade;
};

// 돋보기로 탐색을 시작한다. grade는 기본 돋보기면 BASIC_MAGNIFIER_GRADE, 넓은 탐지 돋보기면 그 등급이다. 서버는 MAGNIFIER_SEARCH_MS 뒤에 결과를 보낸다.
struct C2S_SEARCH_PACKET {
	PACKET_HEADER header;
	std::uint8_t grade;
};

// 끝난 돋보기 탐색의 결과. 탐색한 사람에게만 보낸다. grade는 탐색 요청과 같은 값이고, 위치는 서버가 탐색 시작을 받았을 때의 캡슐 중심(cm, x·y)이다.
// is_found면 그 위치에서 반경(cm) 안에 보물이 있다. 이 결과를 보낼 때 넓은 탐지 돋보기는 하나 쓰고, 기본 돋보기는 재사용 대기시간을 시작한다.
struct S2C_SEARCH_RESULT_PACKET {
	PACKET_HEADER header;
	std::uint8_t grade;
	bool is_found;
	float location[2];
	float radius;
};

// 아무도 보물을 찾지 못한 채 게임 제한시간이 끝났다. 보물 바닥 가운데 위치(cm)와 보물 방향(도)을 플레이 중인 방 전체에 보내고, GAME_END_WAIT_MS 뒤 방으로 돌아간다.
struct S2C_TIME_OVER_PACKET {
	PACKET_HEADER header;
	float location[3];
	float yaw;
};

// 가진 블록 하나를 배제 판에 놓는다. piece는 BLOCK_PIECES 번호, rotation은 시계 방향 90도 회전 수이고, 돌린 모양의 가장 작은 열과 행이 (column, row) 칸에 온다.
// 블록마다 한 번만 쓸 수 있고, 놓는 칸은 모두 판 안이며 아직 배제되지 않은 칸이어야 한다.
struct C2S_BLOCK_USE_PACKET {
	PACKET_HEADER header;
	std::uint8_t piece;
	std::uint8_t rotation;
	std::uint8_t column;
	std::uint8_t row;
};

// 연 상자에 든 블록. piece는 BLOCK_PIECES 번호다. 연 사람에게만 상자 열림 패킷 뒤에 이어 보낸다. 이미 가진 블록이면 더 생기지 않는다.
struct S2C_BLOCK_PACKET {
	PACKET_HEADER header;
	std::uint8_t piece;
};

// 서버가 인정한 블록 사용. 쓴 사람에게만 보내고, 바뀐 배제 판을 이어 보낸다.
// is_inside면 보물이 블록이 놓인 칸 안에 있어 나머지 칸이 모두 배제되고, 아니면 블록이 놓인 칸이 배제된다.
struct S2C_BLOCK_RESULT_PACKET {
	PACKET_HEADER header;
	std::uint8_t piece;
	bool is_inside;
};

// 그 사람의 배제 판. 칸 번호(block_pieces.h) 자리의 비트가 켜진 칸이 배제된 칸이다.
// 가진 방향 힌트와 돋보기 탐색 결과는 배제 영역이 칸 전체를 덮을 때만 그 칸을 배제하고, 블록 결과는 칸 단위로 배제한다. 판이 바뀔 때마다 그 사람에게만 보낸다.
struct S2C_EXCLUSION_BOARD_PACKET {
	PACKET_HEADER header;
	std::uint64_t excluded_cells;
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
