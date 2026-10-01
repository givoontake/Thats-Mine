#pragma once

#include "CoreMinimal.h"
#include "locomotion_cardinal.generated.h"

// 이동 애니메이션을 고르는 기준 방향. 사이 각도는 하체를 돌려 맞춘다.
UENUM(BlueprintType)
enum class ELocomotionCardinal : uint8
{
	FORWARD,
	BACKWARD,
	LEFT,
	RIGHT
};
