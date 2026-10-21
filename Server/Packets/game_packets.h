#pragma once
#include <cstdint>
#include "common_packets.h"
#include "settings.h"

// 히트박스 부위. 서버 판정 데이터와 클라이언트 명중 보고가 같은 값을 쓴다.
enum class HitboxPart : std::uint8_t { HEAD, TORSO, ARM, HAND, LEG, FOOT, COUNT };

// 상자 종류. 서버 후보 데이터와 상자 배치 패킷이 같은 값을 쓴다.
enum class BoxKind : std::uint8_t { TREASURE, GIFT };

// 플레이어 동작. 클라이언트 EPlayerAction과 같은 순서다. RELOAD는 동작 패킷이 아니라 재장전 패킷으로만 오간다.
enum class PlayerAction : std::uint8_t { FIRE, MELEE, RELOAD, PICKUP };

// 이동 상태의 공중 단계. 땅 위, 점프로 올라가는 중, 떨어지는 중(점프 정점 이후·천장에 부딪힌 뒤·걸어서 떨어질 때)이다.
enum class AirPhase : std::uint8_t { GROUND, RISING, FALLING };

// 무기. 클라이언트 EWeaponType과 같은 순서다.
enum class WeaponType : std::uint8_t { PICKAXE, SNIPER, PISTOL, MAGNIFIER };

// 아이템 종류. 등급은 S·A뿐이고 HintGrade 값을 쓴다. 얻으면 그 게임 동안 계속 적용된다.
enum class ItemKind : std::uint8_t { PICKAXE_CHARGE, STUN_GAUGE_DECAY, STUN_GAUGE_DECAY_DELAY, STUN_PROTECTION, COUNT };

// 시상식 부문. 탐색왕은 상자를 가장 많이 연 사람, 기절왕은 다른 플레이어를 가장 많이 기절시킨 사람이다.
enum class AwardCategory : std::uint8_t { BOX_OPEN, STUN, COUNT };
// 부문마다 시상대에 세우는 최대 인원
constexpr std::size_t AWARD_RANK_COUNT = 3;

// 힌트가 말하는 물체 종류. 메시 여러 개를 한 종류로 묶는다.
// 개수 힌트는 모든 종류를 쓰고, DAISY·MUSHROOM은 모든 색을 합친 수다.
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

// 아이템 등급. S가 가장 높다.
enum class HintGrade : std::uint8_t { S, A, B, C };

#pragma pack(push, 1)

// 이동 상태. 위치·속도는 cm, 방향은 도 단위다.
struct MOVE_STATE {
	float location[3];
	float velocity[3];
	float yaw;
	float view_pitch;
	AirPhase air_phase;
	// 이 상태를 만든 클라이언트 게임 시각(ms). 기준은 클라이언트마다 다르고 32비트로 돌아가므로 차이로만 쓴다.
	std::uint32_t client_time_ms;
};

// 0.1초마다, 이동 방향이 크게 꺾일 때, 공중 단계가 바뀔 때, 총을 쏘기 직전에 보낸다.
struct C2S_MOVE_PACKET {
	PACKET_HEADER header;
	MOVE_STATE move_state;
};

// 이동 키 입력이 바뀌었다. 오른쪽·앞쪽 축을 -127~127로 줄여 담는다.
struct C2S_MOVE_INPUT_PACKET {
	PACKET_HEADER header;
	std::int8_t input_right;
	std::int8_t input_forward;
};

// 조준을 시작하거나 끝냈다.
struct C2S_AIM_PACKET {
	PACKET_HEADER header;
	bool is_aiming;
};

// 손에 든 장비를 바꿨다. weapon은 바꾼 뒤 손에 든 WeaponType이다.
struct C2S_EQUIP_PACKET {
	PACKET_HEADER header;
	std::uint8_t weapon;
};

// 다른 플레이어의 이동 상태. 틱마다 중계할 플레이어 수만큼 이어 붙여 보낸다.
// client_time_ms는 그 플레이어 클라이언트가 이 상태를 만든 시각이고, 받은 쪽은 명중을 보낼 때 그 플레이어를 본 시각을 이 기준으로 담는다.
struct S2C_MOVE_PACKET {
	PACKET_HEADER header;
	int player_id;
	float location[3];
	float velocity[3];
	float yaw;
	float view_pitch;
	AirPhase air_phase;
	std::uint32_t client_time_ms;
};

// 다른 플레이어의 이동 키 입력. 받은 쪽은 이 입력으로 그 캐릭터의 이동을 계산한다.
struct S2C_MOVE_INPUT_PACKET {
	PACKET_HEADER header;
	int player_id;
	std::int8_t input_right;
	std::int8_t input_forward;
};

// 다른 플레이어의 조준 여부
struct S2C_AIM_PACKET {
	PACKET_HEADER header;
	int player_id;
	bool is_aiming;
};

// 다른 플레이어가 손에 든 장비
struct S2C_EQUIP_PACKET {
	PACKET_HEADER header;
	int player_id;
	std::uint8_t weapon;
};

// 발사·휘두르기·줍기. 서버는 손에 든 장비로 판정하고, 총 발사는 서버가 탄약을 확인해 인정한 것만 중계한다.
struct C2S_ACTION_PACKET {
	PACKET_HEADER header;
	std::uint8_t action;
};

struct S2C_ACTION_PACKET {
	PACKET_HEADER header;
	int player_id;
	std::uint8_t action;
};

// 재장전을 시작했다. 장비를 바꿔도 이어지므로 손에 든 장비가 아닌 무기일 수 있다. 서버가 탄약을 확인해 인정한 것만 중계한다.
struct C2S_RELOAD_PACKET {
	PACKET_HEADER header;
	std::uint8_t weapon;
};

struct S2C_RELOAD_PACKET {
	PACKET_HEADER header;
	int player_id;
	std::uint8_t weapon;
};

// 로딩을 마친 플레이어 한 명의 스폰 자리. 스폰할 플레이어 수만큼 이어 붙여 보낸다. 이미 플레이 중인 플레이어는 지금 자리다.
// 받는 사람에게는 플레이어마다 이어서 그 사람의 이동 키 입력·조준·장비를 보낸다.
struct S2C_SPAWN_PACKET {
	PACKET_HEADER header;
	int player_id;
	float location[3]; // 캐릭터 캡슐 중심(cm)
	float yaw;
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

// 클라이언트가 총으로 다른 플레이어를 맞혔을 때 보낸다. 광선은 단위 방향·맞은 곳까지의 거리(cm)이고, 시작점은 서버가 이동 상태로 구한다(SHOT_ORIGIN_*). 부위는 클라이언트가 판정한 값이다.
// camera는 발사할 때 카메라 위치(cm)다. aim_direction은 퍼지기 전 조준 방향(단위 벡터), spread는 그 발에 쓴 퍼짐 크기(도)다. 서버는 같은 빗나감을 다시 계산해 광선 방향과 맞춰 본다(shot_spread.h).
// target_time_ms는 쏜 순간 화면의 맞은 사람 시각이다. 마지막으로 받은 그 사람 이동 상태의 client_time_ms에 받은 뒤 지난 시간을 더한다.
struct C2S_HIT_PACKET {
	PACKET_HEADER header;
	int target_player_id;
	std::uint8_t weapon;
	std::uint8_t part;
	float camera[3];
	float direction[3];
	float distance;
	float aim_direction[3];
	float spread;
	std::uint32_t target_time_ms;
};

// 서버가 인정한 명중. 맞은 사람에게만 보내고, 받은 쪽은 쏜 사람 방향을 표시한다.
struct S2C_HIT_PACKET {
	PACKET_HEADER header;
	int shooter_player_id;
};

// 명중으로 바뀐 기절 게이지 값. 맞은 사람에게만 보낸다. 이후 초당 감소는 클라이언트가 계산한다.
struct S2C_STUN_GAUGE_PACKET {
	PACKET_HEADER header;
	float gauge;
};

// 플레이어가 기절했다. 방 전체에 보낸다. 기절은 서버가 기절 해제를 보낼 때 끝난다.
struct S2C_STUN_APPLY_PACKET {
	PACKET_HEADER header;
	int player_id;
};

// 플레이어의 기절이 끝났다. 방 전체에 보낸다. 그 플레이어의 게이지는 0이고, 이어서 protection_ms 동안 보호된다.
struct S2C_STUN_RELEASE_PACKET {
	PACKET_HEADER header;
	int player_id;
	std::uint32_t protection_ms;
};

// 서버가 이동을 인정하지 않아 서버 위치(캡슐 중심, cm)로 맞춘다. 받은 클라이언트는 그 위치로 바로 옮기고 같은 번호로 확인을 보낸다.
// 수평 속도는 없애고, 공중이면 수직 속도(cm/s)로 서버 곡선을 이어 떨어진다.
struct S2C_MOVE_CORRECTION_PACKET {
	PACKET_HEADER header;
	std::uint32_t correction_id;
	float location[3];
	float velocity_z;
	AirPhase air_phase;
};

// 되돌리기를 적용했다는 확인. 서버는 이 확인을 받을 때까지 그 플레이어의 이동 상태를 받지 않는다.
struct C2S_MOVE_CORRECTION_ACK_PACKET {
	PACKET_HEADER header;
	std::uint32_t correction_id;
};

// 상자 하나. 종류는 BoxKind 값이고, 위치는 cm, 회전은 언리얼 회전 순서(pitch, yaw, roll, 도)다.
struct BOX_ENTRY {
	std::uint8_t kind;
	float location[3];
	float rotation[3];
	bool is_opened;
};

// 이번 게임의 상자 배치. 이 구조체 뒤에 box_count개의 BOX_ENTRY가 이어 붙는다.
// 패킷 하나에 다 담지 못하면 이 패킷 여러 개로 나눠 이어 보내고, 상자 번호는 받은 패킷들의 항목을 순서대로 이은 순서다.
// 상자 수에 따라 BUF_SIZE보다 커질 수 있어 상자 배치만 크기에 맞춘 버퍼로 만들어 보낸다.
struct S2C_BOX_LAYOUT_PACKET {
	PACKET_HEADER header;
	std::uint16_t box_count;
};


// 서버가 인정한 발사 뒤 그 무기의 남은 탄창 탄. 쏜 사람에게만 보낸다. 클라이언트는 발사할 때 탄을 줄이지 않고 이 값으로만 바꾼다.
struct S2C_AMMO_USE_PACKET {
	PACKET_HEADER header;
	std::uint8_t weapon;
	std::uint16_t rounds;
};

// 탄약 상자에서 얻은 소지 탄 수. 연 사람에게만 상자 열림 뒤에 보낸다. 클라이언트는 소지 탄에 더하고 획득 안내를 띄운다.
struct S2C_AMMO_GAIN_PACKET {
	PACKET_HEADER header;
	std::uint8_t weapon;
	std::uint16_t rounds;
};

// 서버의 재장전이 끝난 뒤 그 무기의 탄창 탄과 소지 탄. 재장전한 사람에게만 보낸다.
struct S2C_RELOAD_DONE_PACKET {
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

// 상호작용으로 상자를 열려고 한다. 상자 번호는 상자 배치의 항목 순서다.
struct C2S_BOX_OPEN_PACKET {
	PACKET_HEADER header;
	std::uint16_t box;
};

// 쏴서 상자를 열려고 한다. weapon은 쏜 무기이고, 그 무기의 인정된 발사 하나를 쓴다. 선물만 쏴서 열 수 있다.
struct C2S_BOX_SHOT_PACKET {
	PACKET_HEADER header;
	std::uint16_t box;
	std::uint8_t weapon;
};

// 서버가 인정한 상자 열림. 플레이 중인 방 전체에 보낸다.
struct S2C_BOX_OPEN_PACKET {
	PACKET_HEADER header;
	std::uint16_t box;
};

// 얻은 방향 힌트. 상자를 연 사람에게는 상자 열림 뒤에, 기절시켜 힌트를 받은 사람에게는 기절 적용 뒤에 이어 보낸다.
// hint_id는 서버가 이번 게임에 만든 힌트 목록의 번호다. area는 서버 맵 영역 파일의 순서, direction은 HintDirection이고, is_true면 보물이 그 영역보다 그 방위에 있다.
struct S2C_DIRECTION_HINT_PACKET {
	PACKET_HEADER header;
	std::uint16_t hint_id;
	std::uint8_t area;
	std::uint8_t direction;
	bool is_true;
};

// 얻은 개수 힌트. 보내는 때와 hint_id는 방향 힌트와 같다.
// 보물 바닥에서 HINT_OBJECT_COUNT_RADII의 radius번째 반경 안에 있는 object_kind(HintObjectKind) 물체 수(최대 255)다. 거리는 높이까지 넣은 3차원 거리다.
struct S2C_COUNT_HINT_PACKET {
	PACKET_HEADER header;
	std::uint16_t hint_id;
	std::uint8_t object_kind;
	std::uint8_t radius;
	std::uint8_t count;
};

// 기절해서 빼앗긴 힌트의 번호. 기절한 사람에게만 기절 적용 뒤에 이어 보낸다. 받은 클라이언트는 그 번호의 힌트를 인벤토리에서 뺀다.
struct S2C_HINT_LOST_PACKET {
	PACKET_HEADER header;
	std::uint16_t hint_id;
};

// 다음 영역 지우기까지 남은 시간. 게임 시작 때와 지우기 차례가 지날 때마다 플레이 중인 방 전체에 보내고, 게임 도중 합류하면 그 사람에게 보낸다.
struct S2C_AREA_REMOVAL_TIME_PACKET {
	PACKET_HEADER header;
	std::uint32_t remaining_ms;
};

// 영역 지우기 차례에 칸 하나를 지웠다. 플레이 중인 방 전체에 보내고, 판이 바뀐 사람에게는 바뀐 배제 판을 이어 보낸다.
struct S2C_AREA_REMOVAL_PACKET {
	PACKET_HEADER header;
};

// 이번 게임의 탄 퍼짐 시드. 게임 레벨에 들어오는 플레이어에게 스폰 전에 보낸다. 발사 번호는 게임마다 무기별로 0부터 센다.
struct S2C_SHOT_SEED_PACKET {
	PACKET_HEADER header;
	std::uint64_t seed;
};

// 서버가 인정하지 않은 총 발사. 쏜 사람에게만 보내고, 클라이언트는 그 무기의 발사 번호를 하나 되돌린다.
struct S2C_FIRE_REJECTED_PACKET {
	PACKET_HEADER header;
	std::uint8_t weapon;
};

// 시험용 보물 위치 요청. 방장이 요청할 때만 답한다.
struct C2S_TREASURE_REVEAL_PACKET {
	PACKET_HEADER header;
};

// 시험용 보물 위치 요청의 답. 보물 바닥 가운데 위치(cm)다. 플레이 중인 전원에게 보낸다.
struct S2C_TREASURE_REVEAL_PACKET {
	PACKET_HEADER header;
	float location[3];
};

// 보물을 발견한 플레이어. 플레이 중인 방 전체에 보내고, 보물 위치를 이어 보낸다. GAME_END_WAIT_MS 뒤 게임이 끝나고 방으로 돌아간다.
struct S2C_TREASURE_FOUND_PACKET {
	PACKET_HEADER header;
	int player_id;
};

// 보물 바닥 가운데 위치(cm)와 보물 방향(도). 보물 발견이나 시간 종료 뒤에 이어 보낸다. 보물 위치는 그 전에는 보내지 않는다.
struct S2C_TREASURE_LOCATION_PACKET {
	PACKET_HEADER header;
	float location[3];
	float yaw;
};

// 연 상자에 든 아이템. 종류는 ItemKind, 등급은 HintGrade다. 연 사람에게만 상자 열림 패킷 뒤에 이어 보낸다.
struct S2C_ITEM_PACKET {
	PACKET_HEADER header;
	std::uint8_t kind;
	std::uint8_t grade;
};

// 아무도 보물을 찾지 못한 채 게임 제한시간이 끝났다. 플레이 중인 방 전체에 보내고, 보물 위치를 이어 보낸다. GAME_END_WAIT_MS 뒤 방으로 돌아간다.
struct S2C_TIME_OVER_PACKET {
	PACKET_HEADER header;
};

// 시상식 순위 한 자리. count는 그 부문 기록(연 상자 수·기절시킨 수)이다. 빈 자리는 player_id가 -1이다.
struct AWARD_ENTRY {
	int player_id;
	std::uint32_t count;
};

// 시상식 순위 전체. entries는 AwardCategory 순서의 부문마다 1위부터다.
// 부문마다 기록이 1 이상인 사람만 최대 AWARD_RANK_COUNT명이고, 기록이 같으면 그 기록에 먼저 도달한 사람이 위다.
// 게임이 끝나고 GAME_END_WAIT_MS가 지나면 기록이 있는 부문이 하나라도 있을 때 플레이 중인 전원에게 보낸다.
struct S2C_AWARD_PACKET {
	PACKET_HEADER header;
	AWARD_ENTRY entries[static_cast<std::size_t>(AwardCategory::COUNT)][AWARD_RANK_COUNT];
};

// 시상식을 끝까지 봤거나 건너뛰었다. 그 사람만 먼저 방으로 돌려보낸다.
struct C2S_AWARD_DONE_PACKET {
	PACKET_HEADER header;
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

// 연 상자에 든 블록. piece는 BLOCK_PIECES 번호다. 연 사람에게만 상자 열림 패킷 뒤에 이어 보낸다. 이미 가진 블록이면 그 블록이 하나 더 생긴다.
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
// 돋보기 탐색 결과는 배제 영역이 칸 전체를 덮을 때만 그 칸을 배제하고, 블록 결과와 영역 지우기는 칸 단위로 배제한다. 판이 바뀔 때마다 그 사람에게만 보낸다.
struct S2C_EXCLUSION_BOARD_PACKET {
	PACKET_HEADER header;
	std::uint64_t excluded_cells;
};

#pragma pack(pop)
