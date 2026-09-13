#pragma once

#include "CoreMinimal.h"
#include "TimerManager.h"

struct FWeaponAmmoState
{
	int32 rounds = 0;
	FTimerHandle reload_timer;
};
