#pragma once

#include "CoreMinimal.h"
#include "TimerManager.h"

struct FWeaponAmmoState
{
	int32 rounds = 0;
	FTimerHandle reload_timer;
	// 마지막 권총 반동이 끝나기를 기다린다. 재장전 타이머와 따로 두어, 대기가 끝날 때 재장전 타이머를 새로 걸 수 있게 한다.
	FTimerHandle shot_wait_timer;
	bool is_waiting_for_shot = false;
};
