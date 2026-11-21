#pragma once

#include "CoreMinimal.h"
#include "game_packets.h"

// 내 캐릭터 이동을 전송 프레임 하나(MOVE_STEP_MS) 계산한 결과. 이동 동기화가 이 번호의 이동 패킷을 만든다.
struct FLocalMoveStep
{
	uint32 frame_number = 0;
	// 그 번호를 계산한 뒤의 상태
	MOVE_STATE move_state{};
	// 그 번호에 쓴 이동 키(MoveKey 비트). 점프 비트는 그 번호 시작에 뛰었을 때만 켠다.
	uint8 move_keys = 0;
	bool is_aiming = false;
};
