#pragma once

#include "CoreMinimal.h"

// 아이템 종류. 서버 ItemKind와 같은 순서다. 얻으면 그 게임 동안 계속 적용된다.
enum class EItemKind : uint8
{
	PICKAXE_CHARGE,
	STUN_GAUGE_DECAY,
	STUN_GAUGE_DECAY_DELAY,
	STUN_PROTECTION,
	COUNT
};
