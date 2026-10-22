#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>
#include "settings.h"
#include "hitbox_types.h"
#include "weapon_ammo.h"

// 플레이어가 손에 든 장비와, 장비 교체·동작·재장전을 받을지 판단하는 상태. 무기별 탄약과 곡괭이 사용 횟수도 맡는다.
// 동작 시각은 클라이언트가 보낸 서버 기준 시각(ms)이고, 받은 시각은 서버가 요청을 받은 시각(ms)이다.
// 동작 시각이 받은 시각보다 늦거나 직전에 받은 동작 시각보다 이르면 그 요청은 받지 않는다.
class PlayerActionState
{
	std::uint8_t weapon_ = 0;
	// 마지막으로 받은 동작(장비 교체·발사·휘두르기·줍기·재장전)의 동작 시각
	std::uint32_t last_action_ms_ = 0;
	// 다른 무기로 바꾼 동작 시각과 받은 시각. 교체 대기는 동작 시각으로 세고, 교체 뒤 자동 재장전은 받은 시각으로 예약한다.
	std::uint32_t equip_action_ms_ = 0;
	long long equip_receive_ms_ = 0;
	bool has_equip_wait_ = false;
	// 마지막으로 받은 줍기의 동작 시각
	std::uint32_t pickup_action_ms_ = 0;
	bool has_pickup_ = false;
	// 무기 값 순서의 탄약 상태. 게임마다 탄과 탄창 없이 시작한다.
	std::vector<WeaponAmmo> ammo_;
	// 손에 든 무기의 자동 재장전을 시작할 시각(받은 시각 기준, ms). 0이면 예약이 없다.
	long long auto_reload_ms_ = 0;
	// 곡괭이 사용 가능 횟수와, 최대치보다 적을 때 지금 1번이 차기 시작한 시각(ms). 다 차 있으면 0이다.
	int pickaxe_charges_ = PICKAXE_MAX_CHARGES;
	long long pickaxe_charge_start_ms_ = 0;

	// 동작 시각 순서를 확인하고, 받으면 직전 동작 시각을 바꾼다.
	bool AcceptActionTime(std::uint32_t action_time_ms, long long time_ms);
	// 장비 교체 대기 중인 동작 시각인지
	bool IsEquipWaiting(std::uint32_t action_time_ms) const;
	// 손에 든 무기의 탄창이 비었고 소지 탄이 있으면 time_ms(받은 시각 기준)에 자동 재장전을 예약한다. 교체 대기 중이면 대기가 끝날 때로 미룬다.
	void ScheduleAutoReload(long long time_ms);

public:
	// 게임을 시작할 때 무기 수만큼 탄약을 탄과 탄창 없이 만들고, 곡괭이 사용 가능 횟수를 채운다.
	void Reset(std::size_t weapon_count);
	// 방을 나가거나 게임이 끝나 모든 상태를 비운다.
	void Clear();
	// 스폰할 때 손에 든 장비를 처음 무기로 되돌리고 교체 대기와 줍기 기록을 지운다.
	void ResetWeapon();
	// 동작 시각 순서와 무기별 발사 간격 검증의 기준을 동기화로 보낸 서버 시각으로 채운다.
	void ResetActionTime(long long time_ms);

	std::uint8_t GetWeapon() const { return weapon_; }
	const WeaponAmmo* GetAmmo(std::uint8_t weapon) const { return weapon < ammo_.size() ? &ammo_[weapon] : nullptr; }
	int GetPickaxeCharges() const { return pickaxe_charges_; }
	// 다음 1번이 찰 때까지 남은 시간(ms). 다 차 있으면 0이다.
	long long GetPickaxeChargeRemainingMs(long long time_ms, int charge_ms) const;

	// 장비를 바꾼다. 조준 중이면 받지 않는다. 다른 무기로 바뀌면 교체 대기를 시작하고, 이전 무기의 재장전과 자동 재장전 예약을 취소하며,
	// 새 무기의 탄창이 비었고 소지 탄이 있으면 대기가 끝날 때 자동 재장전을 예약한다.
	bool TryEquip(std::uint8_t weapon, bool is_aiming, std::uint32_t action_time_ms, long long time_ms);
	// 손에 든 총을 쏜다. 교체 대기 중, 재장전 중, 탄이 없거나 발사 간격 검증을 통과하지 못하면 값이 없다.
	// 인정하면 탄 하나를 쓰고 발사 번호와 퍼짐 크기를 정한다. 탄창이 비면 발사 동작(fire_motion_ms)이 끝날 때 자동 재장전을 예약한다.
	std::optional<FiredShot> TryFire(const WeaponInfo& info, const ShotSpreadInput& spread_input, std::uint32_t action_time_ms, long long time_ms, long long fire_motion_ms);
	// 곡괭이를 휘두른다. 교체 대기 중이거나 사용 가능 횟수가 없으면 받지 않는다. 최대치에서 쓰면 충전을 시작한다.
	bool TryUsePickaxe(std::uint32_t action_time_ms, long long time_ms, int charge_ms);
	// 곡괭이가 아닌 장비로 휘두른다. 교체 대기 중이면 받지 않는다.
	bool TryMelee(std::uint32_t action_time_ms, long long time_ms);
	// 줍는다. 직전 줍기 동작(pickup_motion_ms)이 끝나기 전이면 받지 않는다.
	bool TryPickup(std::uint32_t action_time_ms, long long time_ms, long long pickup_motion_ms);
	// 손에 든 무기의 재장전을 시작한다. 교체 대기 중, 이미 재장전 중, 소지 탄이 없거나 탄창이 가득 차 있으면 받지 않는다. 자동 재장전 예약은 지운다.
	bool TryStartReload(const WeaponInfo& info, std::uint32_t action_time_ms, long long time_ms);
	// 예약한 자동 재장전 시각이 지났으면 손에 든 무기의 재장전을 시작하고 true를 돌려준다. 그새 시작할 수 없게 됐으면 예약만 지운다.
	bool StartAutoReload(const WeaponInfo& info, long long time_ms);
	// 재장전이 끝날 시각이 지났으면 소지 탄에서 탄창이 빈 만큼 채우고 true를 돌려준다.
	bool FinishReload(std::uint8_t weapon, const WeaponInfo& info, long long time_ms);
	// 소지 탄을 count발 늘린다. 손에 든 무기의 탄창이 비어 있었으면 자동 재장전을 예약한다.
	void AddSpareRounds(std::uint8_t weapon, int count, long long time_ms);
	// 지난 시간만큼 곡괭이 사용 가능 횟수를 채운다. 1번이라도 찼으면 true를 돌려준다.
	bool UpdatePickaxeCharges(long long time_ms, int charge_ms);
};
