#pragma once
#include <cmath>
#include <cstdint>
#include "movement_settings.h"

// 서버와 클라이언트가 같이 쓰는 캐릭터 이동 단계 계산. 충돌은 묻지 않는다.
// 언리얼 캐릭터 이동(걷기·떨어지기)과 같은 처리 순서를 서버와 클라이언트가 각자 진행하면서, 단계마다 자기 충돌 결과를 넣어 부른다.
// 좌표는 언리얼 월드 좌표(cm), 위쪽이 +z다.

// 거의 0으로 보는 값. 언리얼 이동 계산과 같은 기준이다.
constexpr float MOVE_SMALL_NUMBER = 1e-4f;

struct MoveVector {
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
};

inline MoveVector operator+(const MoveVector& a, const MoveVector& b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
inline MoveVector operator-(const MoveVector& a, const MoveVector& b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline MoveVector operator-(const MoveVector& a) { return { -a.x, -a.y, -a.z }; }
inline MoveVector operator*(const MoveVector& a, float scale) { return { a.x * scale, a.y * scale, a.z * scale }; }
inline float Dot(const MoveVector& a, const MoveVector& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline MoveVector Cross(const MoveVector& a, const MoveVector& b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
inline float Length(const MoveVector& a) { return std::sqrt(Dot(a, a)); }
inline float LengthSquared(const MoveVector& a) { return Dot(a, a); }
inline bool IsNearlyZero(const MoveVector& a, float tolerance = 1e-4f) { return std::fabs(a.x) <= tolerance && std::fabs(a.y) <= tolerance && std::fabs(a.z) <= tolerance; }
// 길이가 아주 짧으면 영 벡터
inline MoveVector SafeNormal(const MoveVector& a)
{
	const float length_squared = LengthSquared(a);
	if (length_squared < 1e-8f) return {};
	return a * (1.0f / std::sqrt(length_squared));
}
// 수평 성분만 남긴다.
inline MoveVector Horizontal(const MoveVector& a) { return { a.x, a.y, 0.0f }; }

// 캡슐 쓸기 결과. time은 이동을 0~1로 본 처음 닿은 비율이고, 막히지 않으면 1이다.
// normal은 닿은 점에서 캡슐 쪽을 보는 접촉 법선, impact_normal은 닿은 면 자체의 법선이다.
// 처음부터 겹쳐 있었으면 is_start_penetrating이고, normal과 penetration_depth가 빠져나올 방향과 거리다.
struct MoveHit {
	bool is_blocking = false;
	bool is_start_penetrating = false;
	float time = 1.0f;
	MoveVector trace_start;
	MoveVector trace_end;
	MoveVector location;
	MoveVector impact_point;
	MoveVector normal;
	MoveVector impact_normal;
	float penetration_depth = 0.0f;
};

// 바닥 찾기 결과. floor_distance는 캡슐 아래에서 바닥까지 거리(cm)이고, 줄 검사로 찾았으면 line_distance도 둔다.
struct MoveFloor {
	bool is_blocking = false;
	bool is_walkable = false;
	bool is_line_trace = false;
	float floor_distance = 0.0f;
	float line_distance = 0.0f;
	MoveHit hit;
};

// 걷기에서 속도를 바꿀 때 쓰는 값. 클라이언트 다른 캐릭터는 멈출 자리에 맞춰 세우는 동안 제동을 끄므로 부르는 쪽이 넘긴다.
struct MoveWalkParams {
	float max_speed = MOVE_SPEED_FORWARD;
	float ground_friction = MOVE_GROUND_FRICTION;
	float braking_deceleration = MOVE_BRAKING_DECELERATION;
};

// 이번 단계의 속도와 그 단계 동안 간 거리. 거리는 속도 변화를 시간에 따라 적분한 값이다.
struct MoveStep {
	MoveVector velocity;
	MoveVector delta;
};

// 땅에서 이동 키 입력과 조준 여부로 낼 수 있는 최대 수평 속도(cm/s)
float GetMaxMoveSpeed(std::int8_t input_forward, bool is_aiming);

// 몸 방향(yaw, 도)과 이동 키 입력(-127~127)으로 정한 월드 가속 방향. 길이는 입력 세기(0~1)다.
MoveVector GetMoveInputDirection(float yaw, std::int8_t input_right, std::int8_t input_forward);

// 땅에서 seconds초 동안의 수평 속도 변화와 간 거리. 입력이 없거나 최고 속도를 넘으면 마찰과 제동으로 줄이고,
// 입력이 있으면 속도 방향을 입력 쪽으로 돌린 뒤 가속한다. 가감속과 방향 전환은 지난 시간으로 바로 구한다.
MoveStep CalcWalkStep(const MoveVector& velocity, const MoveVector& input, const MoveWalkParams& params, float seconds);

// 공중에서 seconds초 동안의 속도 변화와 간 거리. 수평 속도는 그대로이고 중력과 낙하 속도 상한만 적용한다.
MoveStep CalcFallStep(const MoveVector& velocity, float seconds);

// 남은 시간에서 이번 단계 시간. 단계가 많이 남았으면 MOVE_MAX_SIMULATION_TIME_STEP 이하로 나눈다.
float GetSimulationTimeStep(float remaining, int iterations);

// 쓸기로 막힌 비율에서 벽에 붙지 않도록 조금 되돌린 비율. delta_length는 이번 이동 길이(cm)다.
float PullBackHitTime(float time, float delta_length);

// 처음부터 겹친 쓸기 결과를 이동 방향이 빠져나가는 쪽이라 무시할지
bool IsMovingOutOfPenetration(const MoveHit& hit, const MoveVector& delta);

// 처음부터 겹쳤을 때 빠져나올 이동
MoveVector GetPenetrationAdjustment(const MoveHit& hit);

// 걸을 수 있는 면에 닿았는지
bool IsWalkable(const MoveHit& hit);

// 닿은 점이 캡슐 중심에서 수평으로 가장자리 여유 안쪽인지. radius는 쓸 때 쓴 캡슐 반지름이다.
bool IsWithinEdgeTolerance(const MoveVector& capsule_location, const MoveVector& impact_point, float radius = CHARACTER_CAPSULE_RADIUS);

// 바닥이 걸을 수 있는 비탈이면 수평 이동을 비탈면을 따라 가도록 위아래로 기울인다.
MoveVector ComputeGroundMovementDelta(const MoveVector& delta, const MoveHit& ramp_hit, bool is_line_trace);

// 면 법선을 따라 미끄러지는 이동
MoveVector ComputeSlideVector(const MoveVector& delta, float time, const MoveVector& normal);

// 공중에서 미끄러져 원래보다 높이 올라가지 않게 한다.
MoveVector HandleSlopeBoosting(const MoveVector& slide, const MoveVector& delta, float time, const MoveVector& normal);

// 걷는 중 벽을 따라 미끄러질 때 쓸 면 법선. 걸을 수 없는 비탈에 밀려 올라가거나, 캡슐 윗부분이 닿아 바닥으로 밀려 내려가지 않게 수평으로 만든다.
MoveVector GetWalkingSlideNormal(const MoveVector& normal, const MoveHit& hit, const MoveVector& delta, const MoveFloor& floor);

// 미끄러지다 두 번째 면에 닿았을 때 다음 이동. 두 면이 90도 이하로 만나면 두 면이 만나는 선을 따라 간다.
MoveVector TwoWallAdjust(const MoveVector& delta, const MoveHit& hit, const MoveVector& old_hit_normal, bool is_walking, const MoveFloor& floor);

// 바닥 거리를 범위 안으로 맞추려면 위로 옮길 거리. 맞출 필요가 없으면 0이다.
float GetFloorHeightAdjustment(const MoveFloor& floor);

// 아래로 쓴 캡슐의 비율로 구한 바닥 거리. 캡슐을 줄여 쓸었으면 줄인 높이를 뺀다.
float GetSweepFloorDistance(float hit_time, float trace_distance, float shrink_height);

// 바닥 찾기에서 쓸 거리. 걷는 중이면 턱 높이와 바닥 거리 상한만큼 더 본다.
float GetFloorSweepDistance(bool is_walking);

// 턱 오르기를 시작할 수 있는지와 오르고 내릴 높이.
// floor_point_z는 지금 서 있는 바닥 점 높이로, 오른 뒤 높이 차이가 턱 높이를 넘는지 볼 때 쓴다.
struct MoveStepUpStart {
	bool can_step_up = false;
	float up_height = 0.0f;
	float down_height = 0.0f;
	float floor_point_z = 0.0f;
};
MoveStepUpStart GetStepUpStart(const MoveVector& location, const MoveHit& hit, const MoveFloor& floor, bool is_walking);

// 턱 위로 내려 디딘 결과가 받아들일 만한지. 오른 높이가 턱 높이를 넘거나, 막힌 면이 앞을 막거나, 가장자리에 걸치면 받아들이지 않는다.
bool IsStepDownAcceptable(const MoveHit& down_hit, const MoveVector& delta, const MoveVector& old_location, float floor_point_z);

// 착지할 수 있는 충돌인지(바닥 찾기 전까지의 검사). 걸을 수 있는 면이 캡슐 아래 반구에 닿고 가장자리에 걸치지 않아야 한다.
bool IsLandingHitAcceptable(const MoveHit& hit);

// 공중에서 막혔을 때 그 자리 바닥을 다시 찾아 착지를 확인할지. 접촉 법선과 면 법선이 다르게 아래 반구에 닿은 경우다.
bool ShouldCheckForValidLandingSpot(const MoveVector& location, const MoveHit& hit);

// 걷기 중 바닥이 끝나는 곳에서 떨어지지 않게 할 때(예측 구간) 쓰는 가장자리의 바깥쪽 수평 방향. 바닥 충돌로 알 수 없으면 영 벡터다.
// 방향을 알면 이동에서 바깥쪽 몫을 뺀 이동(ComputeSlideVector)을, 모르면 이동 방향의 왼쪽·오른쪽(GetLedgeSideMove)을 차례로 본다.
MoveVector GetLedgeNormal(const MoveFloor& ledge_floor);
MoveVector GetLedgeSideMove(const MoveVector& delta, bool is_left);

// 두 바닥 충돌 결과의 처음 막힘이 같은 방향인지(같은 면에 닿았는지) 볼 때 쓰는 비교
bool IsSameNormal(const MoveVector& a, const MoveVector& b);
