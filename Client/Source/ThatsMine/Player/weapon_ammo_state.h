#pragma once

#include "CoreMinimal.h"
#include "TimerManager.h"

struct FWeaponAmmoState
{
	int32 rounds = 0;
	// 소지 탄. 재장전할 때 탄창이 빈 만큼 여기서 채운다.
	int32 spare_rounds = 0;
	FTimerHandle reload_timer;
	// 마지막 권총 반동이 끝나기를 기다린다. 재장전 타이머와 따로 두어, 대기가 끝날 때 재장전 타이머를 새로 걸 수 있게 한다.
	FTimerHandle shot_wait_timer;
	bool is_waiting_for_shot = false;
	// 마지막 발사 동작이 끝나는 월드 시각(초). 서버가 탄창이 비었다고 알렸을 때 권총 반동이 남았는지 본다.
	double fire_end_time = 0.0;
	// 탄 퍼짐(shot_spread.h). 이번 게임에서 쏜 발사 수(다음 발사 번호), 마지막 발사 직후의 연사 열기와 그 월드 시각(초)
	uint32 shot_count = 0;
	int32 spread_heat = 0;
	double spread_time = 0.0;
};
