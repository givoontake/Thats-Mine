#pragma once
#include <cstdint>

// 무기 하나의 탄약 상태. 시각은 서버가 요청을 받은 시각(ms)이고, 동작 시각만 클라이언트가 보낸 서버 기준 시각이다.
struct WeaponAmmo {
	int rounds = 0; // 지금 탄창의 탄
	int spare_rounds = 0; // 소지 탄. 재장전할 때 탄창이 빈 만큼 여기서 채운다.
	long long reload_end_ms = 0; // 재장전이 끝나는 시각. 0이면 재장전 중이 아니다.
	// 발사 간격 검증(settings.h). 직전에 인정한 발사의 동작 시각과 받은 시각, 쌓인 보상(ms). 게임에 들어올 때 동기화 시각으로 채운다.
	std::uint32_t last_fire_action_ms = 0;
	long long last_fire_receive_ms = 0;
	long long fire_reward_ms = 0;
	bool has_unclaimed_shot = false; // 인정한 발사 중 명중·선물 열기에 아직 쓰지 않은 것이 있는지
	// 탄 퍼짐(shot_spread.h). 이번 게임에서 인정한 발사 수, 마지막 발사 직후의 연사 열기와 그 시각,
	// 마지막으로 인정한 발사의 발사 번호와 그 발에 허용하는 퍼짐 크기 최소치(도)
	std::uint32_t shot_count = 0;
	int spread_heat = 0;
	long long spread_time_ms = 0;
	std::uint32_t last_shot_index = 0;
	float last_shot_min_spread = 0.0f;
};
