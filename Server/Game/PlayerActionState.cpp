#include "PlayerActionState.h"
#include "equipment_settings.h"
#include "game_packets.h"

PlayerActionState::PlayerActionState()
	: pickaxe_(PICKAXE_SETTINGS), sniper_(SNIPER_SETTINGS), pistol_(PISTOL_SETTINGS), magnifier_(MAGNIFIER_SETTINGS), equipment_by_weapon_{ &pickaxe_, &sniper_, &pistol_, &magnifier_ }
{
	Reset();
}

void PlayerActionState::Reset()
{
	for (Equipment* equipment : equipment_by_weapon_) equipment->Reset();
}

void PlayerActionState::Clear()
{
	Reset();
	last_action_ms_ = 0;
	ResetWeapon();
}

void PlayerActionState::ResetWeapon()
{
	GetHeldEquipment().OnUnequip();
	for (Equipment* equipment : equipment_by_weapon_) equipment->ClearEquipWait();
	weapon_ = 0;
	has_pickup_ = false;
}

void PlayerActionState::ResetActionTime(long long time_ms)
{
	last_action_ms_ = static_cast<std::uint32_t>(time_ms);
	for (Equipment* equipment : equipment_by_weapon_) equipment->ResetActionTime(time_ms);
}

bool PlayerActionState::AcceptActionTime(std::uint32_t action_time_ms, long long time_ms) const
{
	// 동작 시각은 받은 서버 시각에 흐른 시간을 더한 값이라 서버가 받은 시각보다 늦을 수 없고, 같은 클라이언트의 동작은 시각 순서대로 온다.
	// 시각은 32비트로 돌아가므로 차이로 본다.
	return static_cast<std::int32_t>(action_time_ms - static_cast<std::uint32_t>(time_ms)) <= 0 && static_cast<std::int32_t>(action_time_ms - last_action_ms_) >= 0;
}

Gun* PlayerActionState::FindGun(std::uint8_t weapon)
{
	if (weapon == static_cast<std::uint8_t>(WeaponType::SNIPER)) return &sniper_;
	if (weapon == static_cast<std::uint8_t>(WeaponType::PISTOL)) return &pistol_;
	return nullptr;
}

const Gun* PlayerActionState::FindGun(std::uint8_t weapon) const
{
	if (weapon == static_cast<std::uint8_t>(WeaponType::SNIPER)) return &sniper_;
	if (weapon == static_cast<std::uint8_t>(WeaponType::PISTOL)) return &pistol_;
	return nullptr;
}

bool PlayerActionState::TryEquip(std::uint8_t weapon, std::uint32_t action_time_ms, long long time_ms)
{
	if (weapon >= EQUIPMENT_COUNT || GetHeldEquipment().IsAiming() || !AcceptActionTime(action_time_ms, time_ms)) return false;
	last_action_ms_ = action_time_ms;
	if (weapon == weapon_) return true;
	GetHeldEquipment().OnUnequip();
	weapon_ = weapon;
	GetHeldEquipment().Equip(action_time_ms, time_ms);
	return true;
}

std::optional<FiredShot> PlayerActionState::TryFire(const ShotSpreadInput& spread_input, std::uint32_t action_time_ms, long long time_ms, long long fire_motion_ms)
{
	Gun* gun = FindGun(weapon_);
	if (!gun || !AcceptActionTime(action_time_ms, time_ms)) return std::nullopt;
	const std::optional<FiredShot> shot = gun->TryFire(spread_input, action_time_ms, time_ms, fire_motion_ms);
	if (shot) last_action_ms_ = action_time_ms;
	return shot;
}

bool PlayerActionState::TryMelee(std::uint32_t action_time_ms, long long time_ms)
{
	if (!AcceptActionTime(action_time_ms, time_ms) || !GetHeldEquipment().TryMelee(action_time_ms)) return false;
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

bool PlayerActionState::TryStartReload(std::uint32_t action_time_ms, long long time_ms)
{
	Gun* gun = FindGun(weapon_);
	if (!gun || !AcceptActionTime(action_time_ms, time_ms) || !gun->TryStartReload(action_time_ms)) return false;
	last_action_ms_ = action_time_ms;
	return true;
}

bool PlayerActionState::AddSpareRounds(std::uint8_t weapon, int count, long long time_ms)
{
	Gun* gun = FindGun(weapon);
	if (!gun) return false;
	gun->AddSpareRounds(count);
	// 손에 든 총의 탄창이 비었을 때 소지 탄이 생기면 탄이 떨어졌을 때처럼 재장전한다.
	if (weapon == weapon_) gun->ScheduleAutoReload(time_ms, 0);
	return true;
}

std::array<EquipmentEvent, PlayerActionState::EQUIPMENT_COUNT> PlayerActionState::Update(long long elapsed_ms)
{
	std::array<EquipmentEvent, EQUIPMENT_COUNT> events;
	for (std::size_t weapon = 0; weapon < EQUIPMENT_COUNT; ++weapon) events[weapon] = equipment_by_weapon_[weapon]->Update(elapsed_ms);
	return events;
}
