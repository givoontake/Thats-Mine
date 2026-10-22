#include <algorithm>
#include "PlayerActionState.h"
#include "shot_spread.h"

void PlayerActionState::Reset(std::size_t weapon_count)
{
	ammo_.assign(weapon_count, WeaponAmmo{});
	pickaxe_charges_ = PICKAXE_MAX_CHARGES;
	pickaxe_charge_start_ms_ = 0;
}

void PlayerActionState::Clear()
{
	ammo_.clear();
	pickaxe_charges_ = PICKAXE_MAX_CHARGES;
	pickaxe_charge_start_ms_ = 0;
	ResetWeapon();
}

void PlayerActionState::ResetFireBasis(long long time_ms)
{
	for (WeaponAmmo& ammo : ammo_) {
		ammo.last_fire_action_ms = static_cast<std::uint32_t>(time_ms);
		ammo.last_fire_receive_ms = time_ms;
		ammo.fire_reward_ms = 0;
	}
}

long long PlayerActionState::GetPickaxeChargeRemainingMs(long long time_ms, int charge_ms) const
{
	if (pickaxe_charge_start_ms_ == 0) return 0;
	return charge_ms - (time_ms - pickaxe_charge_start_ms_) % charge_ms;
}

std::optional<FiredShot> PlayerActionState::TryFire(std::uint8_t weapon, const WeaponInfo& info, const ShotSpreadInput& spread_input, long long time_ms, std::uint32_t action_time_ms)
{
	if (weapon >= ammo_.size()) return std::nullopt;
	WeaponAmmo& ammo = ammo_[weapon];
	if (ammo.reload_end_ms != 0 || ammo.rounds <= 0) return std::nullopt;
	// 동작 시각은 받은 서버 시각에 흐른 시간을 더한 값이라 서버가 받은 시각보다 늦을 수 없다. 시각은 32비트로 돌아가므로 차이로 본다.
	if (static_cast<std::int32_t>(action_time_ms - static_cast<std::uint32_t>(time_ms)) > 0) return std::nullopt;
	const long long interval_ms = static_cast<std::int32_t>(action_time_ms - ammo.last_fire_action_ms);
	const long long fire_interval_ms = static_cast<long long>(info.fire_interval * 1000.0f);
	const long long usable_reward_ms = ammo.fire_reward_ms >= 0 ? ammo.fire_reward_ms : std::min(0LL, ammo.fire_reward_ms + FIRE_DEBT_FREE_MS);
	if (interval_ms + usable_reward_ms < fire_interval_ms) return std::nullopt;
	// 간격이 모자라면 모자란 만큼 보상을 쓰고, 남으면 남은 만큼 지난 보상이나 빚을 0 쪽으로 줄인다. 그 뒤 서버가 받은 간격과 동작 시각 간격의 차이를 더한다.
	if (interval_ms < fire_interval_ms) ammo.fire_reward_ms -= fire_interval_ms - interval_ms;
	else if (ammo.fire_reward_ms > 0) ammo.fire_reward_ms = std::max(0LL, ammo.fire_reward_ms - (interval_ms - fire_interval_ms));
	else ammo.fire_reward_ms = std::min(0LL, ammo.fire_reward_ms + (interval_ms - fire_interval_ms));
	ammo.fire_reward_ms = std::min(FIRE_REWARD_MAX_MS, ammo.fire_reward_ms + (time_ms - ammo.last_fire_receive_ms) - interval_ms);
	ammo.last_fire_action_ms = action_time_ms;
	ammo.last_fire_receive_ms = time_ms;
	--ammo.rounds;
	// 서버가 받은 발사 간격은 지연 때문에 실제와 다를 수 있어, 퍼짐 크기는 연사 열기를 더 회복한 값으로 본다.
	const ShotSpreadSettings& spread = GetShotSpreadSettings(weapon);
	const float elapsed_seconds = (time_ms - ammo.spread_time_ms) / 1000.0f;
	const FiredShot shot{ ammo.shot_count++, GetShotSpread(spread, RecoverShotHeat(ammo.spread_heat, elapsed_seconds + SHOT_SPREAD_TIME_ALLOWANCE_SECONDS, info.fire_interval), spread_input.is_aimed, spread_input.horizontal_speed, spread_input.is_falling) };
	ammo.spread_heat = AddShotHeat(spread, RecoverShotHeat(ammo.spread_heat, elapsed_seconds, info.fire_interval));
	ammo.spread_time_ms = time_ms;
	return shot;
}

bool PlayerActionState::TryStartReload(std::uint8_t weapon, const WeaponInfo& info, long long time_ms)
{
	if (weapon >= ammo_.size()) return false;
	WeaponAmmo& ammo = ammo_[weapon];
	if (ammo.reload_end_ms != 0 || ammo.spare_rounds <= 0 || ammo.rounds >= static_cast<int>(info.magazine_capacity)) return false;
	ammo.reload_end_ms = time_ms + static_cast<long long>(info.reload_duration * 1000.0f);
	return true;
}

bool PlayerActionState::FinishReload(std::uint8_t weapon, const WeaponInfo& info, long long time_ms)
{
	if (weapon >= ammo_.size()) return false;
	WeaponAmmo& ammo = ammo_[weapon];
	if (ammo.reload_end_ms == 0 || time_ms < ammo.reload_end_ms) return false;
	ammo.reload_end_ms = 0;
	const int loaded = std::min(static_cast<int>(info.magazine_capacity) - ammo.rounds, ammo.spare_rounds);
	ammo.rounds += loaded;
	ammo.spare_rounds -= loaded;
	return true;
}

void PlayerActionState::AddSpareRounds(std::uint8_t weapon, int count)
{
	if (weapon >= ammo_.size()) return;
	ammo_[weapon].spare_rounds += count;
}

void PlayerActionState::UpdatePickaxeCharges(long long time_ms, int charge_ms)
{
	if (pickaxe_charge_start_ms_ == 0) return;
	const long long gained = (time_ms - pickaxe_charge_start_ms_) / charge_ms;
	if (gained <= 0) return;
	pickaxe_charges_ = static_cast<int>(std::min<long long>(PICKAXE_MAX_CHARGES, pickaxe_charges_ + gained));
	pickaxe_charge_start_ms_ = pickaxe_charges_ == PICKAXE_MAX_CHARGES ? 0 : pickaxe_charge_start_ms_ + gained * charge_ms;
}

bool PlayerActionState::TryUsePickaxe(long long time_ms, int charge_ms)
{
	UpdatePickaxeCharges(time_ms, charge_ms);
	if (pickaxe_charges_ <= 0) return false;
	// 충전은 서버가 받은 시각부터 센다.
	if (pickaxe_charges_ == PICKAXE_MAX_CHARGES) pickaxe_charge_start_ms_ = time_ms;
	--pickaxe_charges_;
	return true;
}
