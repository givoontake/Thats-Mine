#pragma once
#include <cstdint>
#include "game_packets.h"
#include "settings.h"

class MapData;

// 땅에서 이동 키 입력과 조준 여부로 낼 수 있는 최대 수평 속도(cm/s)
inline float GetMaxMoveSpeed(std::int8_t input_forward, bool is_aiming)
{
	if (is_aiming) return MOVE_SPEED_AIMING;
	return input_forward < 0 ? MOVE_SPEED_BACKWARD : MOVE_SPEED_FORWARD;
}

// 이동 상태 state에서 이동 키 입력과 조준 여부를 그대로 둔 채 seconds초 동안 움직인 상태를 언리얼 캐릭터 이동처럼 계산한다.
// 땅에서는 몸 방향 기준 입력으로 가속·마찰·제동하고, 지형과 배치물 바닥을 따라 MOVE_MAX_STEP_HEIGHT까지 오르내리며, 벽에 막히면 벽을 따라 미끄러진다.
// 발밑에 바닥이 없으면 떨어지기 시작하고, 공중에서는 중력으로만 속도가 바뀌다가 바닥에 닿으면 땅에 선다.
MOVE_STATE PredictMove(const MapData& map, const MOVE_STATE& state, std::int8_t input_right, std::int8_t input_forward, bool is_aiming, float seconds);
