#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include "Gun.h"
#include "Magnifier.h"
#include "Pickaxe.h"

// 플레이어가 가진 장비와 손에 든 장비, 장비 교체·동작·재장전을 받을지 판단하는 상태. 장비는 종류마다 하나씩 멤버로 갖고 무기 번호로 찾는다.
// 동작 시각은 클라이언트가 보낸 서버 기준 시각(ms)이고, 받은 시각은 서버가 요청을 받은 시각(ms)이다.
// 동작 시각이 받은 시각보다 늦거나 직전에 받은 동작 시각보다 이르면 그 요청은 받지 않는다.
class PlayerActionState
{
	static constexpr std::size_t EQUIPMENT_COUNT = 4;

	Pickaxe pickaxe_;
	Gun sniper_;
	Gun pistol_;
	Magnifier magnifier_;
	// 무기 번호(WeaponType 순서)로 장비를 찾는다.
	std::array<Equipment*, EQUIPMENT_COUNT> equipment_by_weapon_;
	std::uint8_t weapon_ = 0;
	// 마지막으로 받은 동작(장비 교체·발사·휘두르기·줍기·재장전)의 동작 시각
	std::uint32_t last_action_ms_ = 0;
	// 마지막으로 받은 줍기의 동작 시각
	std::uint32_t pickup_action_ms_ = 0;
	bool has_pickup_ = false;

	// 동작 시각 순서를 확인한다.
	bool AcceptActionTime(std::uint32_t action_time_ms, long long time_ms) const;

public:
	PlayerActionState();

	// 게임을 시작할 때 장비를 모두 이번 게임의 처음 상태로 되돌린다.
	void Reset();
	// 방을 나가거나 게임이 끝나 모든 상태를 비운다.
	void Clear();
	// 스폰할 때 손에 든 장비를 내려놓고 처음 무기로 되돌리며, 교체 대기와 줍기 기록을 지운다.
	void ResetWeapon();
	// 동작 시각 순서와 무기별 발사 간격 검증의 기준을 동기화로 보낸 서버 시각으로 채운다.
	void ResetActionTime(long long time_ms);

	std::uint8_t GetWeapon() const { return weapon_; }
	Equipment& GetHeldEquipment() { return *equipment_by_weapon_[weapon_]; }
	const Equipment& GetHeldEquipment() const { return *equipment_by_weapon_[weapon_]; }
	// 무기 번호의 장비. 번호는 장비 교체에서 인정한 값이어야 한다.
	const Equipment& GetEquipment(std::uint8_t weapon) const { return *equipment_by_weapon_[weapon]; }
	// 무기 번호의 총. 총이 아니면 없다.
	Gun* FindGun(std::uint8_t weapon);
	const Gun* FindGun(std::uint8_t weapon) const;
	Pickaxe& GetPickaxe() { return pickaxe_; }
	const Pickaxe& GetPickaxe() const { return pickaxe_; }

	// 장비를 바꾼다. 없는 무기 번호거나 손에 든 장비가 조준 중이면 받지 않는다. 다른 장비로 바뀌면 이전 장비를 내려놓고 새 장비의 교체 대기를 시작한다.
	bool TryEquip(std::uint8_t weapon, std::uint32_t action_time_ms, long long time_ms);
	// 손에 든 총을 쏜다. 총이 아니거나 총이 받지 않으면 값이 없다.
	std::optional<FiredShot> TryFire(const ShotSpreadInput& spread_input, std::uint32_t action_time_ms, long long time_ms, long long fire_motion_ms);
	// 손에 든 장비로 휘두른다.
	bool TryMelee(std::uint32_t action_time_ms, long long time_ms);
	// 줍는다. 직전 줍기 동작(pickup_motion_ms)이 끝나기 전이면 받지 않는다.
	bool TryPickup(std::uint32_t action_time_ms, long long time_ms, long long pickup_motion_ms);
	// 손에 든 총의 재장전을 시작한다. 총이 아니거나 총이 받지 않으면 거짓이다.
	bool TryStartReload(std::uint32_t action_time_ms, long long time_ms);
	// 그 무기 번호의 총에 소지 탄을 count발 늘린다. 손에 든 총이면 탄창이 비었을 때 자동 재장전을 예약한다. 총이 아니면 거짓이다.
	bool AddSpareRounds(std::uint8_t weapon, int count, long long time_ms);
	// 시간제 장비를 지난 시간(ms)만큼 모두 진행하고, 무기 번호 순서로 일어난 일을 돌려준다.
	std::array<EquipmentEvent, EQUIPMENT_COUNT> Update(long long elapsed_ms);
};
