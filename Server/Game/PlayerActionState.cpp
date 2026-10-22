#include <algorithm>
#include <iterator>
#include "PlayerActionState.h"
#include "shot_spread.h"

// 무기의 장비 교체 대기 시간(ms). 표에 없는 무기는 대기가 없다.
static int GetEquipDurationMs(std::uint8_t weapon)
{
	return weapon < std::size(EQUIP_DURATIONS_MS) ? EQUIP_DURATIONS_MS[weapon] : 0;
}

void PlayerActionState::Reset(std::size_t weapon_count)
{
	ammo_.assign(weapon_count, WeaponAmmo{});
	auto_reload_ms_ = 0;
	pickaxe_charges_ = PICKAXE_MAX_CHARGES;
	pickaxe_charge_start_ms_ = 0;
}

void PlayerActionState::Clear()
{
	ammo_.clear();
	last_action_ms_ = 0;
	pickaxe_charges_ = PICKAXE_MAX_CHARGES;
	pickaxe_charge_start_ms_ = 0;
	ResetWeapon();
}

void PlayerActionState::ResetWeapon()
{
	weapon_ = 0;
	has_equip_wait_ = false;
	has_pickup_ = false;
	auto_reload_ms_ = 0;
}

void PlayerActionState::ResetActionTime(long long time_ms)
{
	last_action_ms_ = static_cast<std::uint32_t>(time_ms);
	for (WeaponAmmo& ammo : ammo_) {
		ammo.last_fire_action_ms = static_cast<std::uint32_t>(time_ms);
		ammo.last_fire_receive_ms = time_ms;
		ammo.fire_reward_ms = 0;
	}
}

bool PlayerActionState::AcceptActionTime(std::uint32_t action_time_ms, long long time_ms)
{
	// 동작 시각은 받은 서버 시각에 흐른 시간을 더한 값이라 서버가 받은 시각보다 늦을 수 없고, 같은 클라이언트의 동작은 시각 순서대로 온다.
	// 시각은 32비트로 돌아가므로 차이로 본다.
	return static_cast<std::int32_t>(action_time_ms - static_cast<std::uint32_t>(time_ms)) <= 0 && static_cast<std::int32_t>(action_time_ms - last_action_ms_) >= 0;
}

bool PlayerActionState::IsEquipWaiting(std::uint32_t action_time_ms) const
{
	return has_equip_wait_ && static_cast<std::int32_t>(action_time_ms - equip_action_ms_) < GetEquipDurationMs(weapon_);
}

void PlayerActionState::ScheduleAutoReload(long long time_ms)
{
	if (auto_reload_ms_ != 0 || weapon_ >= ammo_.size()) return;
	const WeaponAmmo& ammo = ammo_[weapon_];
	if (ammo.rounds > 0 || ammo.spare_rounds <= 0 || ammo.reload_end_ms != 0) return;
	auto_reload_ms_ = has_equip_wait_ ? std::max(time_ms, equip_receive_ms_ + GetEquipDurationMs(weapon_)) : time_ms;
}

long long PlayerActionState::GetPickaxeChargeRemainingMs(long long time_ms, int charge_ms) const
{
	if (pickaxe_charge_start_ms_ == 0) return 0;
	return charge_ms - (time_ms - pickaxe_charge_start_ms_) % charge_ms;
}

bool PlayerActionState::TryEquip(std::uint8_t weapon, bool is_aiming, std::uint32_t action_time_ms, long long time_ms)
{
	if (is_aiming || !AcceptActionTime(action_time_ms, time_ms)) return false;
	last_action_ms_ = action_time_ms;
	if (weapon == weapon_) return true;
	// 장비를 바꾸면 이전 무기의 재장전은 취소된다.
	if (weapon_ < ammo_.size()) ammo_[weapon_].reload_end_ms = 0;
	auto_reload_ms_ = 0;
	weapon_ = weapon;
	has_equip_wait_ = true;
	equip_action_ms_ = action_time_ms;
	equip_receive_ms_ = time_ms;
	ScheduleAutoReload(time_ms);
	return true;
}

std::optional<FiredShot> PlayerActionState::TryFire(const WeaponInfo& info, const ShotSpreadInput& spread_input, std::uint32_t action_time_ms, long long time_ms, long long fire_motion_ms)
{
	if (weapon_ >= ammo_.size() || !AcceptActionTime(action_time_ms, time_ms) || IsEquipWaiting(action_time_ms)) return std::nullopt;
	WeaponAmmo& ammo = ammo_[weapon_];
	if (ammo.reload_end_ms != 0 || ammo.rounds <= 0) return std::nullopt;
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
	last_action_ms_ = action_time_ms;
	--ammo.rounds;
	// 서버가 받은 발사 간격은 지연 때문에 실제와 다를 수 있어, 퍼짐 크기는 연사 열기를 더 회복한 값으로 본다.
	const ShotSpreadSettings& spread = GetShotSpreadSettings(weapon_);
	const float elapsed_seconds = (time_ms - ammo.spread_time_ms) / 1000.0f;
	const FiredShot shot{ ammo.shot_count++, GetShotSpread(spread, RecoverShotHeat(ammo.spread_heat, elapsed_seconds + SHOT_SPREAD_TIME_ALLOWANCE_SECONDS, info.fire_interval), spread_input.is_aimed, spread_input.horizontal_speed, spread_input.is_falling) };
	ammo.spread_heat = AddShotHeat(spread, RecoverShotHeat(ammo.spread_heat, elapsed_seconds, info.fire_interval));
	ammo.spread_time_ms = time_ms;
	// 탄창이 비면 발사 동작이 끝난 뒤 재장전한다.
	if (ammo.rounds == 0) ScheduleAutoReload(time_ms + fire_motion_ms);
	return shot;
}

bool PlayerActionState::TryUsePickaxe(std::uint32_t action_time_ms, long long time_ms, int charge_ms)
{
	if (!AcceptActionTime(action_time_ms, time_ms) || IsEquipWaiting(action_time_ms)) return false;
	UpdatePickaxeCharges(time_ms, charge_ms);
	if (pickaxe_charges_ <= 0) return false;
	last_action_ms_ = action_time_ms;
	// 충전은 서버가 받은 시각부터 센다.
	if (pickaxe_charges_ == PICKAXE_MAX_CHARGES) pickaxe_charge_start_ms_ = time_ms;
	--pickaxe_charges_;
	return true;
}

bool PlayerActionState::TryMelee(std::uint32_t action_time_ms, long long time_ms)
{
	if (!AcceptActionTime(action_time_ms, time_ms) || IsEquipWaiting(action_time_ms)) return false;
	last_action_ms_ = action_time_ms;
	return true;
}

bool PlayerActionState::TryPickup(std::uint32_t action_time_ms, long long time_ms, long long pickup_motion_ms)
{
	if (!AcceptActionTime(action_time_ms, time_ms)) return false;
	if (has_pickup_ && static_cast<std::int32_t>(action_time_ms - pickup_action_ms_) < pickup_motion_ms) return false;
	last_action_ms_ = action_time_ms;
	pickup_action_ms_ = action_time_ms;
	has_pickup_ = true;
	return true;
}

bool PlayerActionState::TryStartReload(const WeaponInfo& info, std::uint32_t action_time_ms, long long time_ms)
{
	if (weapon_ >= ammo_.size() || !AcceptActionTime(action_time_ms, time_ms) || IsEquipWaiting(action_time_ms)) return false;
	WeaponAmmo& ammo = ammo_[weapon_];
	if (ammo.reload_end_ms != 0 || ammo.spare_rounds <= 0 || ammo.rounds >= static_cast<int>(info.magazine_capacity)) return false;
	last_action_ms_ = action_time_ms;
	ammo.reload_end_ms = time_ms + static_cast<long long>(info.reload_duration * 1000.0f);
	auto_reload_ms_ = 0;
	return true;
}

bool PlayerActionState::StartAutoReload(const WeaponInfo& info, long long time_ms)
{
	if (auto_reload_ms_ == 0 || time_ms < auto_reload_ms_) return false;
	const long long start_ms = auto_reload_ms_;
	auto_reload_ms_ = 0;
	if (weapon_ >= ammo_.size()) return false;
	WeaponAmmo& ammo = ammo_[weapon_];
	if (ammo.reload_end_ms != 0 || ammo.spare_rounds <= 0 || ammo.rounds >= static_cast<int>(info.magazine_capacity)) return false;
	ammo.reload_end_ms = start_ms + static_cast<long long>(info.reload_duration * 1000.0f);
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

void PlayerActionState::AddSpareRounds(std::uint8_t weapon, int count, long long time_ms)
{
	if (weapon >= ammo_.size()) return;
	ammo_[weapon].spare_rounds += count;
	// 손에 든 총의 탄창이 비었을 때 소지 탄이 생기면 탄이 떨어졌을 때처럼 재장전한다.
	if (weapon == weapon_) ScheduleAutoReload(time_ms);
}

bool PlayerActionState::UpdatePickaxeCharges(long long time_ms, int charge_ms)
{
	if (pickaxe_charge_start_ms_ == 0) return false;
	const long long gained = (time_ms - pickaxe_charge_start_ms_) / charge_ms;
	if (gained <= 0) return false;
	pickaxe_charges_ = static_cast<int>(std::min<long long>(PICKAXE_MAX_CHARGES, pickaxe_charges_ + gained));
	pickaxe_charge_start_ms_ = pickaxe_charges_ == PICKAXE_MAX_CHARGES ? 0 : pickaxe_charge_start_ms_ + gained * charge_ms;
	return true;
}
