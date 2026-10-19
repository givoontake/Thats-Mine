#pragma once
#include <algorithm>
#include "game_packets.h"
#include "settings.h"

// 서버가 판정하는 공중 상태. 시작점은 상승이면 점프로 떠난 땅 위치, 하강이면 하강을 시작한 위치의 높이(cm)와 그 서버 시각(ms)이다.
// 하강은 시작점에서 속도 0으로 떨어지기 시작한 것으로 본다.
struct AirState {
	AirPhase phase = AirPhase::GROUND;
	float start_z = 0.0f;
	long long start_ms = 0;
};

// 속도 0에서 seconds초 동안 떨어진 거리(cm). 중력대로 빨라지다가 낙하 속도 상한에 닿으면 일정한 속도로 떨어진다. 0초 이하면 0이다.
inline float GetFallDistance(float seconds)
{
	if (seconds <= 0.0f) return 0.0f;
	if (seconds <= TERMINAL_FALL_SECONDS) return 0.5f * GRAVITY * seconds * seconds;
	return TERMINAL_FALL_DISTANCE + TERMINAL_FALL_SPEED * (seconds - TERMINAL_FALL_SECONDS);
}

// 수직 속도 velocity(cm/s, 위가 +)에서 seconds초 동안 자유 낙하로 움직이는 높이(cm). 낙하 속도 상한에 닿으면 그 뒤는 일정한 속도다.
inline float GetFreeFallDisplacement(float velocity, float seconds)
{
	const float to_terminal = (velocity + TERMINAL_FALL_SPEED) / GRAVITY;
	if (seconds <= to_terminal) return velocity * seconds - 0.5f * GRAVITY * seconds * seconds;
	const float accelerating = std::max(0.0f, to_terminal);
	return velocity * accelerating - 0.5f * GRAVITY * accelerating * accelerating - TERMINAL_FALL_SPEED * (seconds - accelerating);
}

inline float GetAirElapsedSeconds(const AirState& air, long long time_ms)
{
	return static_cast<float>(time_ms - air.start_ms) / 1000.0f;
}

// 공중 상태에서 time_ms에 있어야 할 높이(cm)와 수직 속도(cm/s). 지연 여유를 넣지 않은 곡선 값이다.
inline float GetAirHeight(const AirState& air, long long time_ms)
{
	const float elapsed = GetAirElapsedSeconds(air, time_ms);
	if (air.phase == AirPhase::RISING) return air.start_z + JUMP_Z_VELOCITY * elapsed - 0.5f * GRAVITY * elapsed * elapsed;
	return air.start_z - GetFallDistance(elapsed);
}

inline float GetAirVelocityZ(const AirState& air, long long time_ms)
{
	const float elapsed = std::max(0.0f, GetAirElapsedSeconds(air, time_ms));
	if (air.phase == AirPhase::RISING) return JUMP_Z_VELOCITY - GRAVITY * elapsed;
	return -std::min(GRAVITY * elapsed, TERMINAL_FALL_SPEED);
}

// 받은 높이 z가 공중 상태의 허용 범위 안인지 본다. 상승은 위쪽만 막고, 내려가기 시작하면 클라이언트가 하강으로 알린다.
// 하강은 곡선을 MOVE_AIR_DELAY_SECONDS만큼 늦춘 높이가 위쪽, 당긴 높이가 아래쪽 한계이고, 둘 다 MOVE_TOLERANCE만큼 넓힌다.
inline bool IsAirHeightAllowed(const AirState& air, float z, long long time_ms)
{
	if (air.phase == AirPhase::RISING) return z <= air.start_z + JUMP_APEX_HEIGHT + MOVE_TOLERANCE;
	const float elapsed = GetAirElapsedSeconds(air, time_ms);
	return z <= air.start_z - GetFallDistance(elapsed - MOVE_AIR_DELAY_SECONDS) + MOVE_TOLERANCE
		&& z >= air.start_z - GetFallDistance(elapsed + MOVE_AIR_DELAY_SECONDS) - MOVE_TOLERANCE;
}
