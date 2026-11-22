#pragma once

#include "CoreMinimal.h"
#include "TimerManager.h"

class UAudioComponent;

struct FWeaponAmmoState
{
	int32 rounds = 0;
	// 소지 탄. 재장전할 때 탄창이 빈 만큼 여기서 채운다.
	int32 spare_rounds = 0;
	// 재장전 중인지와 시작한 월드 시각(초). 끝은 서버가 재장전 끝으로 알리고, 장비를 바꾸면 취소된다.
	bool is_reloading = false;
	double reload_start_time = 0.0;
	// 재생 중인 재장전 소리. 이 무기의 재장전이 끝나거나 취소되면 함께 멈춘다.
	TWeakObjectPtr<UAudioComponent> reload_sound;
	// 탄 퍼짐(shot_spread.h). 마지막 발사 직후의 연사 열기와 그 발사의 전송 프레임 번호. 서버와 같이 번호 차이로 회복한다.
	int32 spread_heat = 0;
	uint32 spread_frame_number = 0;
};
