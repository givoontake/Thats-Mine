#pragma once

#include "CoreMinimal.h"
#include "inventory_hint.h"

// 인벤토리에 보이는 아이템 하나. 문장은 아이템 효과다.
struct FInventoryItem
{
	EHintGrade grade = EHintGrade::A;
	FText text;
};
