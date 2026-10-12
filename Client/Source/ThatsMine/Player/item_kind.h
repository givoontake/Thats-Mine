#pragma once

#include "CoreMinimal.h"

// 아이템 종류. 서버 ItemKind와 같은 순서다. 돋보기 앞까지는 얻으면 계속 적용되는 아이템이다.
enum class EItemKind : uint8
{
	PICKAXE_CHARGE,
	STUN_GAUGE_DECAY,
	STUN_GAUGE_DECAY_DELAY,
	STUN_PROTECTION,
	MAGNIFIER,
	COUNT
};
