#pragma once

// 무기 하나의 탄약 상태. 시각은 방 틱 시각(ms)이다.
struct WeaponAmmo {
	int rounds = 0; // 지금 탄창의 탄
	int spare_rounds = 0; // 소지 탄. 재장전할 때 탄창이 빈 만큼 여기서 채운다.
	long long reload_end_ms = 0; // 재장전이 끝나는 시각. 0이면 재장전 중이 아니다.
	long long next_fire_ms = 0; // 다음 발사를 받을 수 있는 시각
	bool has_unclaimed_shot = false; // 인정한 발사 중 명중·선물 열기에 아직 쓰지 않은 것이 있는지
};
