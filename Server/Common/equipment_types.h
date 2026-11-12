#pragma once
#include "shot_spread.h"

// 장비 종류마다 정해진 값. 값은 equipment_settings.h의 상수에 두고, 서버 장비와 클라이언트 무기 설정이 자기 종류의 상수를 참조로 읽는다.
struct EquipmentSettings {
	int equip_duration_ms; // 장비 교체 대기. 다른 장비에서 이 장비로 바꾼 동작 시각부터 이 시간(ms) 동안 발사·휘두르기·재장전을 받지 않는다.
	float stun_gauge_gain; // 이 장비로 맞혔을 때 오르는 기절 게이지. 맞은 부위 배율을 곱한다.
	bool can_aim; // 조준할 수 있는지
};

// 총에만 있는 값
struct GunSettings : EquipmentSettings {
	float hitscan_distance; // 사거리(cm)
	int magazine_capacity; // 탄창 하나의 탄 수
	int reload_duration_ms; // 재장전 시간(ms)
	int fire_interval_ms; // 발사 간격(ms)
	ShotSpreadSettings spread; // 탄 퍼짐 수치
};

// 곡괭이에만 있는 값. 게임마다 최대 사용 횟수로 시작하고, 최대치보다 적으면 충전 시간(ms)마다 1번씩 찬다.
struct PickaxeSettings : EquipmentSettings {
	int max_charges;
	int charge_ms;
};
