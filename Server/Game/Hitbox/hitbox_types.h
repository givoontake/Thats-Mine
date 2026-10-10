#pragma once
#include <cstdint>
#include <vector>
#include <DirectXMath.h>
#include "game_packets.h"

// 판정용 애니메이션 키 간격. 내보내기 도구와 같은 값이다.
constexpr float ANIMATION_SAMPLE_RATE = 30.0f;
// 서버만 쓰는 동작 값. 무기를 바꿀 때 재생하는 꺼내기 동작이다.
constexpr std::uint8_t ACTION_EQUIP = 255;
// 무기와 상관없이 재생하는 동작의 무기 값
constexpr std::uint8_t ANY_WEAPON = 255;

// 뼈 하나의 자세. 애니메이션 키에서는 부모 뼈 기준(루트는 액터 기준), 계층 계산 뒤에는 액터 기준이다. 위치는 cm다.
struct BoneTransform {
	DirectX::XMFLOAT4 rotation;
	DirectX::XMFLOAT3 position;
};

// 이동 애니메이션을 고르는 기준 방향. 사이 각도는 하체를 돌려 맞춘다. 클라이언트와 같은 규칙이다.
enum class LocomotionCardinal : std::uint8_t { FORWARD, BACKWARD, LEFT, RIGHT };

enum class HitboxShapeType : std::uint8_t { SPHERE, CAPSULE, BOX };

// 히트박스 도형 하나. transform은 뼈 기준(월드 도형에서는 월드 기준)이다.
// 캡슐은 회전한 Z축 방향의 원통(길이 length)과 양 끝 반구이고, 상자는 회전한 축마다 half_size만큼 퍼진다.
struct HitboxShape {
	HitboxShapeType type;
	HitboxPart part;
	BoneTransform transform;
	float radius;
	float length;
	DirectX::XMFLOAT3 half_size;
};

struct HitboxBody {
	int bone;
	std::vector<HitboxShape> shapes;
};

// 뼈 하나에 묶인 정점을 기본 자세에서 감싸는 구. 중심은 뼈 기준(cm)이고, 정점이 없는 뼈는 반지름 0이다.
struct BoneSphere {
	DirectX::XMFLOAT3 center;
	float radius;
};

// 1차 판정 캡슐. 캐릭터 위아래 축과 평행한 세로 캡슐이고, 축은 캐릭터 기준(cm)으로 앞(x)·오른쪽(y)으로 치우친 곳을 지나는 높이 bottom_z~top_z의 선분이다.
struct BroadphaseCapsule {
	float offset_x;
	float offset_y;
	float bottom_z;
	float top_z;
	float radius;
};

// 애니메이션 하나를 30Hz로 뽑은 키. keys는 [키 번호 * 뼈 수 + 뼈 번호]다.
struct AnimationTrack {
	float length;
	int key_count;
	std::vector<BoneTransform> keys;
};

// 이동 블렌드 스페이스 샘플. speed는 cm/s, direction은 이동 방향(도)이다.
struct LocomotionSample {
	float speed;
	float direction;
	int track;
	float rate_scale;
};

// 애니메이션 블루프린트가 상체에 덮는 고정 자세. 파일에 이 순서로 들어 있다.
enum class UpperBodyPose : std::uint8_t { IDLE, PICKAXE_HOLD, PISTOL_AIM, RIFLE_HOLD, RIFLE_AIM, COUNT };

enum class WeaponPoseType : std::uint8_t { NONE, PICKAXE, PISTOL, RIFLE };

// 무기 값 순서대로 둔 무기 정보. 상체 자세 종류, 조준 상하 제한(도), 총(광선 판정) 여부와 사거리(cm)다.
struct WeaponInfo {
	WeaponPoseType pose_type;
	float aim_pitch_up_limit;
	float aim_pitch_down_limit;
	bool is_hitscan;
	float hitscan_distance;
	std::uint32_t magazine_capacity; // 탄창 하나의 탄 수
	float reload_duration; // 재장전 시간(초)
	float fire_interval; // 발사 간격(초)
};

// FULL_BODY: 전신, UPPER_BODY: 허리부터 위, RIGHT_ARM: 오른팔
enum class MontageRegion : std::uint8_t { FULL_BODY, UPPER_BODY, RIGHT_ARM };

// 동작과 무기에 맞춰 재생하는 몽타주. 재생 속도와 섞임 시간(초)은 클라이언트 설정과 같다.
struct MontageInfo {
	std::uint8_t action;
	std::uint8_t weapon;
	int track;
	float play_rate;
	float blend_in;
	float blend_out;
	MontageRegion region;
};

// 광선과 히트박스가 처음 만난 결과. distance는 광선 시작점에서 맞은 곳까지의 거리(cm)다.
struct HitboxRayHit {
	bool has_hit = false;
	float distance = 0.0f;
	HitboxPart part = HitboxPart::TORSO;
};
