#pragma once
#include <cstdint>
#include <optional>
#include <vector>
#include "hitbox_types.h"
#include "game_packets.h"

class HitboxData;

// 플레이어 한 명의 서버 판정용 자세. 이동 상태와 동작 이벤트로 애니메이션 진행을 따라가고, 요청한 시각의 히트박스를 월드 좌표로 만든다.
// 클라이언트 애니메이션 블루프린트를 상태별 대표 혼합으로 재현한다. 하체는 이동 블렌드 스페이스와 하체 돌리기, 상체는 무기 자세와 조준 상하 각도,
// 고개는 시선 방향, 동작은 몽타주를 해당 부위에 덮는다. 상체 흔들림, 왼손 IK, 상태 전환 섞임은 생략한다.
class CharacterPose
{
	const HitboxData* data_ = nullptr;
	LocomotionCardinal cardinal_ = LocomotionCardinal::FORWARD;
	float locomotion_phase_ = 0.0f; // 이동 애니메이션의 정규화 재생 위치(0~1). 블렌드 스페이스 샘플이 같은 위치를 함께 쓴다.
	float jump_time_ = 0.0f;
	std::uint8_t weapon_ = 0;
	bool has_weapon_ = false;
	const MontageInfo* montage_ = nullptr;
	long long montage_start_ms_ = 0;

public:
	void Reset(const HitboxData& data);
	// 매 틱 이동 상태로 이동 재생 위치와 점프 시간을 진행한다. 무기가 바뀌면 꺼내기 동작을 시작한다.
	void Update(const MOVE_STATE& move_state, float delta_seconds, long long time_ms);
	// 동작 이벤트를 받은 시각부터 동작과 무기에 맞는 몽타주를 재생한다. 맞는 몽타주가 없으면 재생 중인 동작을 그대로 둔다.
	void StartAction(std::uint8_t action, std::uint8_t weapon, long long time_ms);
	// time_ms 시각의 뼈 자세를 액터 기준(캐릭터 위치·좌우 방향 적용 전)으로 계산한다.
	std::vector<BoneTransform> BuildBonePoses(const MOVE_STATE& move_state, long long time_ms) const;
	// time_ms 시각의 히트박스를 월드 좌표로 만든다. 도형의 transform이 월드 기준이 된다.
	std::vector<HitboxShape> BuildWorldHitboxes(const MOVE_STATE& move_state, long long time_ms) const;
	// time_ms 시각에 bone 뼈가 있을 월드 위치. 자세를 만들 수 없으면 비어 있다.
	std::optional<DirectX::XMFLOAT3> GetBoneLocation(const MOVE_STATE& move_state, int bone, long long time_ms) const;
};
