#pragma once

#include "CoreMinimal.h"

// 키를 바꿀 수 있는 동작. 설정 조작 탭에 이 순서로 보인다. 마우스 왼쪽·오른쪽 클릭과 ESC는 바꿀 수 없다.
// 저장된 키는 이 순서의 번호로 읽으므로 새 동작은 COUNT 바로 앞에 붙인다.
enum class EKeyAction : uint8
{
	MOVE_FORWARD,
	MOVE_BACKWARD,
	MOVE_LEFT,
	MOVE_RIGHT,
	JUMP,
	INTERACT,
	PICKUP,
	RELOAD,
	WEAPON_SNIPER,
	WEAPON_PISTOL,
	WEAPON_PICKAXE,
	WEAPON_MAGNIFIER,
	INVENTORY,
	MAP,
	HELP,
	PING,
	COUNT
};
