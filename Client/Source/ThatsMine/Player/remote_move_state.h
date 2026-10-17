#pragma once

#include "CoreMinimal.h"
#include "weapon_type.h"
#include "game_packets.h"

// 이동 동기화로 주고받는 캐릭터 상태. 이동 입력은 X가 오른쪽, Y가 앞쪽이다.
struct FRemoteMoveState
{
	FVector location = FVector::ZeroVector;
	FVector velocity = FVector::ZeroVector;
	float yaw = 0.0f;
	float view_pitch = 0.0f;
	FVector2D move_input = FVector2D::ZeroVector;
	AirPhase air_phase = AirPhase::GROUND;
	bool is_aiming = false;
	EWeaponType weapon = EWeaponType::PICKAXE;
};
