#pragma once
#include "Equipment.h"

// 곡괭이. 휘두를 때마다 사용 횟수를 하나 쓰고, 최대치보다 적으면 충전 시간마다 1번씩 찬다.
class Pickaxe : public Equipment
{
	int charges_ = 0; // 남은 사용 횟수
	long long charge_progress_ms_ = 0; // 다음 1번이 찰 때까지 쌓인 시간(ms). 다 차 있으면 0이다.
	int charge_ms_ = 0; // 지금 1번 충전 시간(ms). 게임마다 기본값으로 시작하고 아이템으로 짧아진다.

	const PickaxeSettings& GetPickaxeSettings() const { return static_cast<const PickaxeSettings&>(GetSettings()); }

public:
	explicit Pickaxe(const PickaxeSettings& settings);

	int GetCharges() const { return charges_; }
	// 다음 1번이 찰 때까지 남은 시간(ms). 다 차 있으면 0이다.
	long long GetChargeRemainingMs() const;
	// 충전 시간을 charge_ms로 바꾼다. 지금보다 짧을 때만 바꾼다.
	void ShortenChargeMs(int charge_ms);

	void Reset() override;
	// 휘두른다. 교체 대기 중이거나 사용 횟수가 없으면 받지 않는다. 최대치에서 쓰면 충전을 시작한다.
	bool TryMelee(std::uint32_t action_time_ms) override;
	// 쌓인 시간만큼 사용 횟수를 채운다. 1번이라도 차면 CHARGE다.
	EquipmentEvent Update(long long elapsed_ms) override;
};
