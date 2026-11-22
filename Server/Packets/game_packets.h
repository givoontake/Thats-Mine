#pragma once
#include <cstdint>
#include "common_packets.h"

// 히트박스 부위. 서버 판정 데이터의 부위 값이다.
enum class HitboxPart : std::uint8_t { HEAD, TORSO, ARM, HAND, LEG, FOOT, COUNT };

// 상자 종류. 서버 후보 데이터와 상자 배치 패킷이 같은 값을 쓴다.
enum class BoxKind : std::uint8_t { TREASURE, GIFT };

// 플레이어 동작. 클라이언트 EPlayerAction과 같은 순서다. RELOAD는 동작 패킷이 아니라 재장전 패킷으로만 오가고, FIRE는 발사 패킷으로 받아 동작 패킷으로 중계한다.
enum class PlayerAction : std::uint8_t { FIRE, MELEE, RELOAD, PICKUP };

// 이동 키. 앞뒤·좌우 이동 키와 점프다. 이동 패킷의 키 칸은 이 값 자리의 비트다.
enum class MoveKey : std::uint8_t { FORWARD, BACKWARD, LEFT, RIGHT, JUMP, COUNT };

// 무기. 클라이언트 EWeaponType과 같은 순서다.
enum class WeaponType : std::uint8_t { PICKAXE, SNIPER, PISTOL, MAGNIFIER };
// 탄을 쏘는 무기인지. 클라이언트가 손에 든 무기 번호로 총 여부를 판단한다.
constexpr bool IsGunWeapon(WeaponType weapon) { return weapon == WeaponType::SNIPER || weapon == WeaponType::PISTOL; }

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
	// 공중이면 참. 이 상태에서 이어 계산할 때 걷기로 할지 떨어지기로 할지 정한다.
	// 속도로 대신하지 않는 것은, 걷는 동안 수직 속도는 늘 0이지만 공중에서도 단계 끝 수직 속도가 정확히 0일 수 있기 때문이다
	// (단계 끝에 맞춰 바닥 끝을 벗어난 순간, 천장에 부딪힌 순간, 점프 정점).
	bool is_falling;
};

// 전송 프레임 번호를 담는 게임 패킷(이동·조준·장비·동작·재장전·발사·상자 열기·블록 놓기)은 헤더 바로 뒤에 frame_number를 둔다.
// 전송 프레임은 MOVE_STEP_MS 길이의 구간이고, 번호는 내 캐릭터 스폰을 받은 순간 0에서 시작해 MOVE_STEP_MS마다 1씩 는다.
// 클라이언트는 게임 프레임을 시작할 때 읽은 시각으로 번호를 정해 그 게임 프레임에서 만든 패킷에 넣고, 번호가 끝난 뒤 첫 게임 프레임에 그 번호의 패킷 뒤에 이동 패킷을 붙여 보낸다.
// 서버는 번호마다 그 번호의 패킷을 받은 순서대로(이동 패킷이 마지막) 한 틱에 처리한다.

// 전송 프레임마다 하나. 게임 프레임이 없던 번호도 보낸다. move_state는 그 번호의 MOVE_STEP_MS를 계산한 뒤의 상태다.
// 번호의 마지막 패킷이라, 서버는 이것을 받으면 그 번호가 다 온 것으로 본다.
struct C2S_MOVE_PACKET {
	PACKET_HEADER header;
	std::uint32_t frame_number;
	MOVE_STATE move_state;
	// 그 번호의 마지막 게임 프레임에 눌려 있던 이동 키(MoveKey 자리의 비트). 점프 비트는 그 번호 안에서 한 번이라도 뛰려고 했고 번호 시작에 뛰었을 때 켠다.
	std::uint8_t move_keys;
};

// 조준을 시작하거나 끝냈다. 서버는 이 번호의 이동부터 바뀐 조준으로 계산한다.
struct C2S_AIM_PACKET {
	PACKET_HEADER header;
	std::uint32_t frame_number;
	bool is_aiming;
};

// 손에 든 장비를 바꿨다. weapon은 바꾼 뒤 손에 든 WeaponType이다.
// 서버는 조준 중이면 받지 않고, 다른 무기로 바뀌면 이 번호부터 그 장비의 교체 대기(equip_duration_ms) 동안 발사·휘두르기·재장전을 받지 않는다.
struct C2S_EQUIP_PACKET {
	PACKET_HEADER header;
	std::uint32_t frame_number;
	std::uint8_t weapon;
};

// 다른 플레이어의 이동 상태. 틱마다 이동 상태가 바뀐 플레이어 수만큼 이어 붙여 보낸다.
// 받은 쪽은 그 캐릭터를 지금 그린 위치에서 받은 위치까지 MOVE_STEP_MS 동안 옮긴다.
struct S2C_MOVE_PACKET {
	PACKET_HEADER header;
	int player_id;
	float location[3];
	float velocity[3];
	float yaw;
	float view_pitch;
	bool is_falling;
	// 이 상태의 방 틱 번호. 발사 패킷에 대상의 틱 번호로 돌려 보낸다.
	std::uint32_t tick_number;
};

// 다른 플레이어의 조준 여부. 서버가 조준 변경을 받지 않으면 바꾸려던 본인에게 지금 조준 여부를 보내 맞추게 한다.
struct S2C_AIM_PACKET {
	PACKET_HEADER header;
	int player_id;
	bool is_aiming;
};

// 다른 플레이어가 손에 든 장비. 서버가 장비 교체를 받지 않으면 바꾸려던 본인에게 지금 장비를 보내 맞추게 한다.
struct S2C_EQUIP_PACKET {
	PACKET_HEADER header;
	int player_id;
	std::uint8_t weapon;
};

// 휘두르기·줍기. 서버는 손에 든 장비로 판정한다. 총 발사는 C2S_FIRE_PACKET으로 보낸다.
struct C2S_ACTION_PACKET {
	PACKET_HEADER header;
	std::uint32_t frame_number;
	std::uint8_t action;
};

// 손에 든 총을 쐈다. 서버가 탄약과 발사 간격을 확인해 인정하면 다른 플레이어에게 발사 동작을 중계하고, 탄 광선을 직접 판정한다.
// aim_direction은 퍼지기 전 조준 방향(단위 벡터)이고, camera는 발사할 때 카메라 위치(cm)다.
// shot_count는 클라이언트가 그 무기로 쏜 횟수다. 빗나감 방향은 (플레이어 아이디, 무기, shot_count)로 정하고(shot_spread.h), 서버는 앞 발사보다 클 때만 받는다.
// 클라이언트는 같은 빗나감으로 탄을 만들어 맞은 대상을 찾고, 그 대상이 화면에서 향하던 틱 번호와 그 틱까지 간 정도를 보낸다. 빗나갔으면 대상은 0이다.
// 서버는 모든 플레이어를 그 시점으로 되감아 판정한다.
struct C2S_FIRE_PACKET {
	PACKET_HEADER header;
	std::uint32_t frame_number;
	float aim_direction[3];
	float camera[3];
	std::uint32_t shot_count;
	int target_player_id;
	// 대상이 화면에서 향하던 방 틱 번호와, 앞 위치에서 그 틱 위치까지 간 정도(0~100)
	std::uint32_t target_tick;
	std::uint8_t target_progress;
};

struct S2C_ACTION_PACKET {
	PACKET_HEADER header;
	int player_id;
	std::uint8_t action;
};

// 손에 든 무기의 재장전을 시작했다. 장비를 바꾸면 재장전은 취소된다.
// 서버가 인정하면 재장전 중계를 보내고, 인정하지 않으면 재장전한 사람에게 지금 탄약으로 재장전 끝을 바로 보낸다.
struct C2S_RELOAD_PACKET {
	PACKET_HEADER header;
	std::uint32_t frame_number;
};

// 손에 든 총의 재장전을 발사 입력으로 취소했다. 탄창에 탄이 남아 있을 때만 보낸다.
// 서버는 그 번호에 손에 든 총이 weapon이고 재장전 중이며 탄창에 탄이 있을 때만 취소한다.
struct C2S_RELOAD_CANCEL_PACKET {
	PACKET_HEADER header;
	std::uint32_t frame_number; // 이 입력이 든 전송 프레임 번호
	std::uint8_t weapon; // 재장전을 취소한 총(WeaponType)
};

// 재장전이 시작됐다. 방 전체에 보내고 재장전한 사람도 받는다. 서버가 자동 재장전을 시작했을 때도 보낸다.
struct S2C_RELOAD_PACKET {
	PACKET_HEADER header;
	int player_id;
	std::uint8_t weapon;
};

// 그 플레이어가 그 총의 재장전을 취소했다. 본인을 뺀 플레이어에게 보낸다. 받은 쪽은 그 캐릭터의 재장전 동작을 멈춘다.
struct S2C_RELOAD_CANCEL_PACKET {
	PACKET_HEADER header;
	int player_id;
	std::uint8_t weapon; // 재장전을 취소한 총(WeaponType)
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
// 내 캐릭터를 스폰했다. 서버는 이것을 받은 뒤부터 그 플레이어의 이동 패킷을 처리한다.
struct C2S_SPAWN_COMPLETE_PACKET {
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

// 서버가 이동을 인정하지 않아 서버 이동 상태로 롤백한다. 방 안 플레이어 모두에게 보낸다.
// 받은 클라이언트는 그 캐릭터를 보간 없이 위치(캡슐 중심, cm)·속도(cm/s)·방향으로 바로 바꾼다.
// 롤백된 본인은 이 상태를 지금 이동 상태로 세팅하고 확인을 보낸다. 아직 보내지 않은 패킷은 버린다.
struct S2C_MOVE_ROLLBACK_PACKET {
	PACKET_HEADER header;
	int player_id;
	float location[3];
	float velocity[3];
	float yaw;
	float view_pitch;
	// 공중이면 참. 받은 클라이언트가 이 상태에서 이어 계산할 때 걷기로 할지 떨어지기로 할지 정한다.
	// 속도로 대신하지 않는 것은, 걷는 동안 수직 속도는 늘 0이지만 공중에서도 단계 끝 수직 속도가 정확히 0일 수 있기 때문이다
	// (단계 끝에 맞춰 바닥 끝을 벗어난 순간, 천장에 부딪힌 순간, 점프 정점).
	bool is_falling;
	// 롤백한 방 틱 번호
	std::uint32_t tick_number;
};

// 롤백을 적용했다는 확인. 롤백을 받은 즉시 보낸다. 서버는 확인 전에 온 게임 패킷을 버리고,
// 확인 뒤 첫 이동 패킷에서 그 번호를 처리할 틱을 다시 정한다.
// 롤백은 확인을 받기 전에 다시 보내지 않으므로 기다리는 롤백은 늘 하나이고, 연결이 순서를 지키므로 번호 없이 그 롤백의 확인이다.
struct C2S_MOVE_ROLLBACK_ACK_PACKET {
	PACKET_HEADER header;
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

// 서버가 아는 곡괭이 사용 가능 횟수와 다음 1번이 찰 때까지 남은 시간(ms, 다 차 있으면 0). 그 사람에게만 보낸다.
// 곡괭이 휘두르기를 인정했을 때, 인정하지 않았을 때, 1번이 찼을 때 보낸다. 클라이언트는 이 값으로만 횟수를 바꾸고 남은 시간은 표시만 센다.
struct S2C_PICKAXE_CHARGES_PACKET {
	PACKET_HEADER header;
	std::uint8_t charges;
	std::uint32_t remaining_ms;
};

// 상호작용으로 상자를 열려고 한다. 상자 번호는 상자 배치의 항목 순서다. camera는 카메라 위치(cm), view_direction은 카메라가 보는 방향(단위 벡터)이다.
// 서버는 카메라 위치를 사격 카메라와 같이 검증하고, 시선이 그 상자의 상호작용 범위에 닿으며 카메라에서 그곳까지 지형에 가리지 않아야 연다.
struct C2S_BOX_OPEN_PACKET {
	PACKET_HEADER header;
	std::uint32_t frame_number;
	std::uint16_t box;
	float camera[3];
	float view_direction[3];
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

// 서버가 그 플레이어를 멈춘 것으로 판단했다. 이동 상태가 MOVE_STOP_DELAY_SECONDS 넘게 오지 않았거나 서버가 이동을 되돌렸을 때 방 전체에 보낸다.
// 받은 쪽은 그 캐릭터를 location으로 옮기고 이동 키 입력을 없는 것으로 두어 수평으로 바로 멈춘다. 공중이면 떨어지는 것은 이어 간다.
// 이동 상태가 다시 오면 서버가 그 사람의 이동 상태를 다시 중계한다.
struct S2C_MOVE_STOP_PACKET {
	PACKET_HEADER header;
	int player_id;
	float location[3]; // 서버가 정한 멈춘 위치(캡슐 중심)
	// 멈춘 방 틱 번호
	std::uint32_t tick_number;
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
	std::uint32_t frame_number;
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
