#pragma once
#include <cstdint>
#include <optional>
#include "Equipment.h"

// 퍼짐 크기를 정할 쏜 사람의 이동 상태. 공중 여부와 수평 속도(cm/s)는 서버 이동 상태의 값이다.
struct ShotSpreadInput {
	bool is_falling;
	float horizontal_speed;
};

// 인정한 발사 하나. 이번 게임에서 그 무기의 발사 번호와 서버가 정한 퍼짐 크기(도)다.
struct FiredShot {
	std::uint32_t shot_index;
	float spread;
};

// 총. 저격총과 권총이 같은 클래스를 쓰고 참조하는 고유값만 다르다. 탄약, 재장전, 발사 간격 검증, 탄 퍼짐, 조준 상태를 든다.
class Gun : public Equipment
{
	int rounds_ = 0; // 지금 탄창의 탄
	int spare_rounds_ = 0; // 소지 탄. 재장전할 때 탄창이 빈 만큼 여기서 채운다.
	long long reload_remaining_ms_ = 0; // 재장전이 끝날 때까지 남은 시간. 0이면 재장전 중이 아니다.
	// 자동 재장전을 예약했는지와 시작할 때까지 남은 시간(ms)
	bool has_auto_reload_ = false;
	long long auto_reload_wait_ms_ = 0;
	// 발사 간격 검증(combat_settings.h). 직전에 인정한 발사의 동작 시각과 받은 시각, 쌓인 보상(ms). 게임에 들어올 때 동기화 시각으로 채운다.
	// 받은 시각은 연사 열기를 회복하는 기준이기도 하다.
	std::uint32_t last_fire_action_ms_ = 0;
	long long last_fire_receive_ms_ = 0;
	long long fire_reward_ms_ = 0;
	// 탄 퍼짐(shot_spread.h). 이번 게임에서 인정한 발사 수와 마지막 발사 직후의 연사 열기
	std::uint32_t shot_count_ = 0;
	int spread_heat_ = 0;
	// 조준 중인지
	bool is_aiming_ = false;

public:
	explicit Gun(const GunSettings& settings);

	const GunSettings& GetGunSettings() const { return static_cast<const GunSettings&>(GetSettings()); }
	int GetRounds() const { return rounds_; }
	int GetSpareRounds() const { return spare_rounds_; }

	// 탄과 소지 탄 없이 시작한다.
	void Reset() override;
	void ResetActionTime(long long time_ms) override;
	// 탄창이 비었고 소지 탄이 있으면 교체 대기가 끝날 때 자동 재장전을 예약한다.
	void Equip(std::uint32_t action_time_ms, long long time_ms) override;
	// 재장전과 자동 재장전 예약을 취소하고 조준을 푼다.
	void OnUnequip() override;
	// 재장전이 끝나면 소지 탄에서 탄창이 빈 만큼 채우고, 예약한 자동 재장전 시간이 지나면 재장전을 시작한다.
	EquipmentEvent Update(long long elapsed_ms) override;
	bool IsAiming() const override { return is_aiming_; }
	bool TrySetAiming(bool is_aiming) override;

	// 쏜다. 교체 대기 중, 재장전 중, 탄이 없거나 발사 간격 검증을 통과하지 못하면 값이 없다.
	// 인정하면 탄 하나를 쓰고 발사 번호와 퍼짐 크기를 정한다. 탄창이 비면 발사 동작(fire_motion_ms)이 끝날 때 자동 재장전을 예약한다.
	std::optional<FiredShot> TryFire(const ShotSpreadInput& spread_input, std::uint32_t action_time_ms, long long time_ms, long long fire_motion_ms);
	// 재장전을 시작한다. 교체 대기 중, 이미 재장전 중, 소지 탄이 없거나 탄창이 가득 차 있으면 받지 않는다. 자동 재장전 예약은 지운다.
	bool TryStartReload(std::uint32_t action_time_ms);
	// 소지 탄을 count발 늘린다.
	void AddSpareRounds(int count) { spare_rounds_ += count; }
	// 탄창이 비었고 소지 탄이 있으면, 받은 시각 time_ms에서 delay_ms와 남은 교체 대기 중 긴 쪽이 지난 뒤로 자동 재장전을 예약한다.
	void ScheduleAutoReload(long long time_ms, long long delay_ms);
};
