#pragma once
#include <cstdint>
#include <optional>
#include "game_packets.h"
#include "shared_movement.h"

class MapData;

// 서버가 정한 한 플레이어의 이동 상태와, 그 상태를 이동 키 입력대로 진행하는 계산. 언리얼 캐릭터 이동의 걷기·떨어지기와 같은 순서로 단계를 나누어,
// 단계마다 MapData 충돌(랜드스케이프와 캐릭터를 막는 배치물)을 묻고 그 결과를 공용 이동 계산(SharedMovement)에 넣는다.
// 진행한 상태를 들고 있어 이어 부르면 앞 진행의 끝에서 이어 간다.
// 서버는 받은 이동 상태 뒤를 키 입력으로 이어 가므로, 걷다가 바닥 끝에 닿으면 떨어지지 않고 가장자리에 선다. 공중으로 넘어가는 것은 클라이언트가 알린다.
class MoveSimulation
{
	const MapData* map_ = nullptr;
	MoveVector location_;
	MoveVector velocity_;
	// 몸 방향(yaw, 도)과 시선 상하(도). 이동 키 입력의 방향을 몸 방향으로 정한다.
	float yaw_ = 0.0f;
	float view_pitch_ = 0.0f;
	// 월드 가속 방향. 길이는 입력 세기(0~1)다.
	MoveVector input_;
	MoveWalkParams walk_params_;
	// 걷는 중 밟고 있는 바닥. 떨어지는 중에는 비어 있다.
	MoveFloor floor_;
	bool is_walking_ = false;
	// 이번 단계에서 위치를 바로 옮겨, 간 거리로 속도를 다시 정하지 않을지
	bool is_just_teleported_ = false;
	// 한 번 진행하는 동안 점프 정점에서 단계를 나눈 횟수
	int jump_apex_attempts_ = 0;

	MoveHit Move(const MoveVector& delta, bool is_ignoring_moving_out);
	MoveHit SafeMove(const MoveVector& delta);
	bool ResolvePenetration(const MoveVector& adjustment, const MoveHit& hit);
	struct SlideResult {
		float percent;
		MoveHit hit;
	};
	SlideResult SlideAlongSurface(const MoveVector& delta, float time, const MoveVector& normal, const MoveHit& hit);
	MoveFloor FindFloor(const MoveVector& location, const MoveHit* downward_sweep) const;
	void AdjustFloorHeight();
	bool IsValidLandingSpot(const MoveVector& location, const MoveHit& hit) const;
	// 턱을 올랐는지와, 오른 뒤 내려 디딘 곳에서 찾은 바닥(디딘 곳이 없으면 비어 있다)
	struct StepUpResult {
		bool is_stepped;
		std::optional<MoveFloor> floor;
	};
	StepUpResult StepUp(const MoveVector& delta, const MoveHit& hit);
	// 바닥을 따라 간다. 턱을 오르며 바닥을 찾았으면 그 바닥을 돌려준다.
	std::optional<MoveFloor> MoveAlongFloor(const MoveVector& delta);
	bool CheckLedgeDirection(const MoveVector& old_location, const MoveVector& side_step) const;
	MoveVector GetLedgeMove(const MoveVector& old_location, const MoveVector& delta, const MoveFloor& old_floor) const;
	void StartFalling(int iterations, float remaining, float tick, const MoveVector& delta, const MoveVector& old_location);
	void ProcessLanded(float remaining, int iterations);
	void StartNewPhysics(float seconds, int iterations);
	void PhysWalking(float seconds, int iterations);
	void PhysFalling(float seconds, int iterations);

public:
	MoveSimulation() = default;
	// state(위치·속도·몸 방향·시선·공중 단계)에서 시작한다. 걷는 중이면 그 자리 바닥을 찾아 둔다.
	MoveSimulation(const MapData& map, const MOVE_STATE& state);
	// 이동 키 입력과 조준 여부를 그대로 둔 채 seconds초 진행한다.
	void Simulate(std::int8_t input_right, std::int8_t input_forward, bool is_aiming, float seconds);
	// 수평으로 바로 멈춘다. 공중이면 떨어지는 것은 이어 간다.
	void StopHorizontal();
	// 몸 방향과 시선 상하를 바꾼다. 위치와 속도는 그대로다.
	void SetView(float yaw, float view_pitch);
	bool IsWalking() const { return is_walking_; }
	// 걷는 중이고 찾아 둔 바닥이 걸을 수 있는 바닥인지. 바닥이 없으면 다음 진행에서 떨어지기 시작한다.
	bool IsOnWalkableFloor() const { return is_walking_ && floor_.is_walkable; }
	// 지금 이동 상태. 클라이언트 시각은 0이다.
	MOVE_STATE GetMoveState() const;

	// 세로로 선 캡슐을 중심 start에서 end까지 옮길 때 랜드스케이프나 캐릭터를 막는 배치물에 처음 닿는 곳
	static MoveHit SweepWorld(const MapData& map, const MoveVector& start, const MoveVector& end, float radius, float half_height, bool is_ignoring_moving_out);
	// 세로로 선 캡슐이 랜드스케이프나 캐릭터를 막는 배치물과 겹치는지
	static bool IsOverlappingWorld(const MapData& map, const MoveVector& center, float radius, float half_height);
	// 캐릭터 캡슐 중심 location 아래의 바닥. 캡슐을 sweep_distance까지 아래로 쓸고, 쓸기가 걸을 수 없는 면에 닿았으면 중심에서 line_distance까지 선분으로 다시 본다.
	// downward_sweep은 방금 아래로 쓴 결과로, 그대로 바닥으로 쓸 수 있으면 쓸기를 다시 하지 않는다.
	static MoveFloor ComputeFloorDist(const MapData& map, const MoveVector& location, float line_distance, float sweep_distance, float sweep_radius, const MoveHit* downward_sweep);
};
