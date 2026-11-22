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
	state_ = GunState::READY;
	reload_remaining_ms_ = 0;
	has_auto_reload_ = false;
	auto_reload_wait_ms_ = 0;
	last_fire_action_ms_ = 0;
	shot_count_ = 0;
	spread_heat_ = 0;
	is_aiming_ = false;
}

void Gun::ResetActionTime(std::uint32_t action_time_ms)
{
	last_fire_action_ms_ = action_time_ms;
}

void Gun::Equip(std::uint32_t action_time_ms)
{
	Equipment::Equip(action_time_ms);
	ScheduleAutoReload(action_time_ms, 0);
}

void Gun::OnUnequip()
{
	// 장비를 바꾸면 재장전은 취소된다.
	state_ = GunState::READY;
	reload_remaining_ms_ = 0;
	has_auto_reload_ = false;
	is_aiming_ = false;
}

EquipmentEvent Gun::Update(long long elapsed_ms)
{
	if (state_ == GunState::RELOADING) {
		reload_remaining_ms_ -= elapsed_ms;
		if (reload_remaining_ms_ > 0) return EquipmentEvent::NONE;
		state_ = GunState::READY;
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
	state_ = GunState::RELOADING;
	reload_remaining_ms_ = GetGunSettings().reload_duration_ms;
	return EquipmentEvent::RELOAD_START;
}

bool Gun::TrySetAiming(bool is_aiming)
{
	if (is_aiming && !GetSettings().can_aim) return false;
	is_aiming_ = is_aiming;
	return true;
}

std::optional<FiredShot> Gun::TryFire(const ShotSpreadInput& spread_input, std::uint32_t shot_count, std::uint32_t action_time_ms, long long fire_motion_ms)
{
	if (IsEquipWaiting(action_time_ms) || state_ == GunState::RELOADING || rounds_ <= 0 || shot_count <= shot_count_) return std::nullopt;
	const GunSettings& gun = GetGunSettings();
	// 직전 인정 발사와의 동작 시각 간격이 발사 간격보다 짧으면 받지 않는다.
	if (static_cast<std::int32_t>(action_time_ms - last_fire_action_ms_) < gun.fire_interval_ms) return std::nullopt;
	const float elapsed_seconds = static_cast<std::int32_t>(action_time_ms - last_fire_action_ms_) / 1000.0f;
	last_fire_action_ms_ = action_time_ms;
	shot_count_ = shot_count;
	--rounds_;
	// 퍼짐 크기는 연사 열기를 SHOT_SPREAD_TIME_ALLOWANCE_SECONDS만큼 더 회복한 값으로 본다.
	const FiredShot shot{ GetShotSpread(gun.spread, RecoverShotHeat(spread_heat_, elapsed_seconds + SHOT_SPREAD_TIME_ALLOWANCE_SECONDS, gun.fire_interval_ms), is_aiming_, spread_input.horizontal_speed, spread_input.is_falling) };
	spread_heat_ = AddShotHeat(gun.spread, RecoverShotHeat(spread_heat_, elapsed_seconds, gun.fire_interval_ms));
	// 탄창이 비면 발사 동작이 끝난 뒤 재장전한다.
	if (rounds_ == 0) ScheduleAutoReload(action_time_ms, fire_motion_ms);
	return shot;
}

bool Gun::TryStartReload(std::uint32_t action_time_ms)
{
	if (IsEquipWaiting(action_time_ms) || state_ == GunState::RELOADING || spare_rounds_ <= 0 || rounds_ >= GetGunSettings().magazine_capacity) return false;
	state_ = GunState::RELOADING;
	reload_remaining_ms_ = GetGunSettings().reload_duration_ms;
	has_auto_reload_ = false;
	return true;
}

bool Gun::TryCancelReload()
{
	// 탄창이 비었으면 재장전이 끝날 때까지 발사 입력을 받지 않는다.
	if (state_ != GunState::RELOADING || rounds_ <= 0) return false;
	state_ = GunState::READY;
	reload_remaining_ms_ = 0;
	return true;
}

void Gun::ScheduleAutoReload(std::uint32_t action_time_ms, long long delay_ms)
{
	if (has_auto_reload_ || rounds_ > 0 || spare_rounds_ <= 0 || state_ == GunState::RELOADING) return;
	has_auto_reload_ = true;
	auto_reload_wait_ms_ = std::max(delay_ms, GetEquipWaitRemainingMs(action_time_ms));
}
