#include <algorithm>
#include "Equipment.h"

Equipment::Equipment(const EquipmentSettings& settings)
	: settings_(settings)
{
}

long long Equipment::GetEquipWaitRemainingMs(std::uint32_t action_time_ms) const
{
	if (!has_equip_wait_) return 0;
	// 동작 시각은 32비트로 돌아가므로 차이로 본다.
	const std::int32_t remaining_ms = static_cast<std::int32_t>(equip_action_ms_ + static_cast<std::uint32_t>(settings_.equip_duration_ms) - action_time_ms);
	return std::max(0, remaining_ms);
}

bool Equipment::IsEquipWaiting(std::uint32_t action_time_ms) const
{
	return has_equip_wait_ && static_cast<std::int32_t>(action_time_ms - equip_action_ms_) < settings_.equip_duration_ms;
}

void Equipment::Equip(std::uint32_t action_time_ms)
{
	equip_action_ms_ = action_time_ms;
	has_equip_wait_ = true;
}

bool Equipment::TryMelee(std::uint32_t action_time_ms)
{
	return !IsEquipWaiting(action_time_ms);
}
