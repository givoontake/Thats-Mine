#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>
#include "settings.h"
#include "hitbox_types.h"
#include "weapon_ammo.h"

// 플레이어가 손에 든 장비와 무기별 탄약, 곡괭이 사용 횟수
class PlayerActionState
{
	std::uint8_t weapon_ = 0;
	// 무기 값 순서의 탄약 상태. 게임마다 탄과 탄창 없이 시작한다.
	std::vector<WeaponAmmo> ammo_;
	// 곡괭이 사용 가능 횟수와, 최대치보다 적을 때 지금 1번이 차기 시작한 시각(ms). 다 차 있으면 0이다.
	int pickaxe_charges_ = PICKAXE_MAX_CHARGES;
	long long pickaxe_charge_start_ms_ = 0;

	// 지난 시간만큼 곡괭이 사용 가능 횟수를 채운다.
	void UpdatePickaxeCharges(long long time_ms, int charge_ms);

public:
	// 게임을 시작할 때 무기 수만큼 탄약을 탄과 탄창 없이 만들고, 곡괭이 사용 가능 횟수를 채운다.
	void Reset(std::size_t weapon_count);
	// 방을 나가거나 게임이 끝나 모든 상태를 비운다.
	void Clear();
	// 스폰할 때 손에 든 장비를 처음 무기로 되돌린다.
	void ResetWeapon() { weapon_ = 0; }
	// 모든 무기의 발사 간격 검증 기준을 동기화로 보낸 서버 시각으로 채우고 보상을 비운다.
	void ResetFireBasis(long long time_ms);

	std::uint8_t GetWeapon() const { return weapon_; }
	void SetWeapon(std::uint8_t weapon) { weapon_ = weapon; }
	const WeaponAmmo* GetAmmo(std::uint8_t weapon) const { return weapon < ammo_.size() ? &ammo_[weapon] : nullptr; }
	int GetPickaxeCharges() const { return pickaxe_charges_; }
	// 다음 1번이 찰 때까지 남은 시간(ms). 다 차 있으면 0이다.
	long long GetPickaxeChargeRemainingMs(long long time_ms, int charge_ms) const;

	// 재장전 중이 아니고 탄이 있고 발사 간격 검증을 통과하면 탄 하나를 쓰고 인정한다. 인정하면 발사 번호와 그 발의 퍼짐 크기를 정하고 연사 누적을 늘린다.
	// time_ms는 서버가 받은 시각, action_time_ms는 클라이언트가 보낸 동작 시각이다. 인정하지 않으면 값이 없다.
	std::optional<FiredShot> TryFire(std::uint8_t weapon, const WeaponInfo& info, const ShotSpreadInput& spread_input, long long time_ms, std::uint32_t action_time_ms);
	// 재장전 중이 아니고 탄창이 가득 차 있지 않고 소지 탄이 있으면 재장전을 시작한다.
	bool TryStartReload(std::uint8_t weapon, const WeaponInfo& info, long long time_ms);
	// 재장전이 끝날 시각이 지났으면 소지 탄에서 탄창이 빈 만큼 채우고 true를 돌려준다.
	bool FinishReload(std::uint8_t weapon, const WeaponInfo& info, long long time_ms);
	// 소지 탄을 count발 늘린다.
	void AddSpareRounds(std::uint8_t weapon, int count);
	// 사용 가능 횟수가 있으면 1번 쓰고 인정한다. 최대치에서 쓰면 충전을 시작한다.
	bool TryUsePickaxe(long long time_ms, int charge_ms);
};
