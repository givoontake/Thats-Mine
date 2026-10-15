#pragma once

#include "CoreMinimal.h"

// 다른 플레이어에게 애니메이션만 재생하는 동작. 값은 동작 패킷에 그대로 실린다.
enum class EPlayerAction : uint8
{
	FIRE,
	MELEE,
	RELOAD,
	PICKUP
};
