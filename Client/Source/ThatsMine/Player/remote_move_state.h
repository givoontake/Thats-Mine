#pragma once

#include "CoreMinimal.h"
#include "weapon_type.h"
#include "game_packets.h"

// 이동 동기화로 주고받는 캐릭터 이동 상태. 이동 키 입력·조준·장비는 바뀔 때 따로 주고받는다.
struct FRemoteMoveState
{
	FVector location = FVector::ZeroVector;
	FVector velocity = FVector::ZeroVector;
	float yaw = 0.0f;
	float view_pitch = 0.0f;
	AirPhase air_phase = AirPhase::GROUND;
};
