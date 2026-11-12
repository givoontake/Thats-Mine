#include <algorithm>
#include "Gun.h"
#include "combat_settings.h"
#include "shot_spread.h"

Gun::Gun(const GunSettings& settings)
	: Equipment(settings)
{
}

void Gun::Reset()
{
	rounds_ = 0;
	spare_rounds_ = 0;
	reload_remaining_ms_ = 0;
	has_auto_reload_ = false;
	auto_reload_wait_ms_ = 0;
	last_fire_action_ms_ = 0;
	last_fire_receive_ms_ = 0;
	fire_reward_ms_ = 0;
	shot_count_ = 0;
	spread_heat_ = 0;
	is_aiming_ = false;
	aim_change_ms_ = 0;
}

void Gun::ResetActionTime(long long time_ms)
{
	last_fire_action_ms_ = static_cast<std::uint32_t>(time_ms);
	last_fire_receive_ms_ = time_ms;
	fire_reward_ms_ = 0;
}

void Gun::Equip(std::uint32_t action_time_ms, long long time_ms)
{
	Equipment::Equip(action_time_ms, time_ms);
	ScheduleAutoReload(time_ms, 0);
}

void Gun::OnUnequip()
{
	// 장비를 바꾸면 재장전은 취소된다.
	reload_remaining_ms_ = 0;
	has_auto_reload_ = false;
	is_aiming_ = false;
}

EquipmentEvent Gun::Update(long long elapsed_ms)
{
	if (reload_remaining_ms_ > 0) {
		reload_remaining_ms_ -= elapsed_ms;
		if (reload_remaining_ms_ > 0) return EquipmentEvent::NONE;
		reload_remaining_ms_ = 0;
		const int loaded = std::min(GetGunSettings().magazine_capacity - rounds_, spare_rounds_);
		rounds_ += loaded;
		spare_rounds_ -= loaded;
		return EquipmentEvent::RELOAD_DONE;
	}
	if (!has_auto_reload_) return EquipmentEvent::NONE;
	auto_reload_wait_ms_ -= elapsed_ms;
	if (auto_reload_wait_ms_ > 0) return EquipmentEvent::NONE;
	has_auto_reload_ = false;
	// 그새 시작할 수 없게 됐으면 예약만 지운다.
	if (spare_rounds_ <= 0 || rounds_ >= GetGunSettings().magazine_capacity) return EquipmentEvent::NONE;
	reload_remaining_ms_ = GetGunSettings().reload_duration_ms;
	return EquipmentEvent::RELOAD_START;
}

bool Gun::TrySetAiming(bool is_aiming, long long time_ms)
{
	if (is_aiming && !GetSettings().can_aim) return false;
	if (is_aiming != is_aiming_) aim_change_ms_ = time_ms;
	is_aiming_ = is_aiming;
	return true;
}

std::optional<FiredShot> Gun::TryFire(const ShotSpreadInput& spread_input, std::uint32_t action_time_ms, long long time_ms, long long fire_motion_ms)
{
	if (IsEquipWaiting(action_time_ms) || reload_remaining_ms_ > 0 || rounds_ <= 0) return std::nullopt;
	const GunSettings& gun = GetGunSettings();
	const long long interval_ms = static_cast<std::int32_t>(action_time_ms - last_fire_action_ms_);
	const long long fire_interval_ms = gun.fire_interval_ms;
	const long long usable_reward_ms = fire_reward_ms_ >= 0 ? fire_reward_ms_ : std::min(0LL, fire_reward_ms_ + FIRE_DEBT_FREE_MS);
	if (interval_ms + usable_reward_ms < fire_interval_ms) return std::nullopt;
	// 간격이 모자라면 모자란 만큼 보상을 쓰고, 남으면 남은 만큼 지난 보상이나 빚을 0 쪽으로 줄인다. 그 뒤 서버가 받은 간격과 동작 시각 간격의 차이를 더한다.
	if (interval_ms < fire_interval_ms) fire_reward_ms_ -= fire_interval_ms - interval_ms;
	else if (fire_reward_ms_ > 0) fire_reward_ms_ = std::max(0LL, fire_reward_ms_ - (interval_ms - fire_interval_ms));
	else fire_reward_ms_ = std::min(0LL, fire_reward_ms_ + (interval_ms - fire_interval_ms));
	fire_reward_ms_ = std::min(FIRE_REWARD_MAX_MS, fire_reward_ms_ + (time_ms - last_fire_receive_ms_) - interval_ms);
	const float elapsed_seconds = (time_ms - last_fire_receive_ms_) / 1000.0f;
	last_fire_action_ms_ = action_time_ms;
	last_fire_receive_ms_ = time_ms;
	--rounds_;
	// 서버가 받은 조준 변화와 발사 간격은 지연 때문에 실제와 다를 수 있어, 막 바뀐 조준은 조준한 것으로 보고 퍼짐 크기는 연사 열기를 더 회복한 값으로 본다.
	const bool is_aimed = is_aiming_ || time_ms - aim_change_ms_ < SHOT_SPREAD_STATE_ALLOWANCE_MS;
	const FiredShot shot{ shot_count_++, GetShotSpread(gun.spread, RecoverShotHeat(spread_heat_, elapsed_seconds + SHOT_SPREAD_TIME_ALLOWANCE_SECONDS, gun.fire_interval_ms), is_aimed, spread_input.horizontal_speed, spread_input.is_falling) };
	spread_heat_ = AddShotHeat(gun.spread, RecoverShotHeat(spread_heat_, elapsed_seconds, gun.fire_interval_ms));
	// 탄창이 비면 발사 동작이 끝난 뒤 재장전한다.
	if (rounds_ == 0) ScheduleAutoReload(time_ms, fire_motion_ms);
	return shot;
}

bool Gun::TryStartReload(std::uint32_t action_time_ms)
{
	if (IsEquipWaiting(action_time_ms) || reload_remaining_ms_ > 0 || spare_rounds_ <= 0 || rounds_ >= GetGunSettings().magazine_capacity) return false;
	reload_remaining_ms_ = GetGunSettings().reload_duration_ms;
	has_auto_reload_ = false;
	return true;
}

void Gun::ScheduleAutoReload(long long time_ms, long long delay_ms)
{
	if (has_auto_reload_ || rounds_ > 0 || spare_rounds_ <= 0 || reload_remaining_ms_ > 0) return;
	has_auto_reload_ = true;
	auto_reload_wait_ms_ = std::max(delay_ms, GetEquipWaitRemainingMs(time_ms));
}
