#include <algorithm>
#include "Pickaxe.h"

Pickaxe::Pickaxe(const PickaxeSettings& settings)
	: Equipment(settings)
{
}

long long Pickaxe::GetChargeRemainingMs() const
{
	if (charges_ >= GetPickaxeSettings().max_charges) return 0;
	return std::max(0LL, charge_ms_ - charge_progress_ms_);
}

void Pickaxe::ShortenChargeMs(int charge_ms)
{
	if (charge_ms < charge_ms_) charge_ms_ = charge_ms;
}

void Pickaxe::Reset()
{
	charges_ = GetPickaxeSettings().max_charges;
	charge_progress_ms_ = 0;
	charge_ms_ = GetPickaxeSettings().charge_ms;
}

bool Pickaxe::TryMelee(std::uint32_t action_time_ms)
{
	if (!Equipment::TryMelee(action_time_ms) || charges_ <= 0) return false;
	if (charges_ == GetPickaxeSettings().max_charges) charge_progress_ms_ = 0;
	--charges_;
	return true;
}

EquipmentEvent Pickaxe::Update(long long elapsed_ms)
{
	const int max_charges = GetPickaxeSettings().max_charges;
	if (charges_ >= max_charges) return EquipmentEvent::NONE;
	charge_progress_ms_ += elapsed_ms;
	const long long gained = charge_progress_ms_ / charge_ms_;
	if (gained <= 0) return EquipmentEvent::NONE;
	charges_ = static_cast<int>(std::min<long long>(max_charges, charges_ + gained));
	charge_progress_ms_ = charges_ == max_charges ? 0 : charge_progress_ms_ - gained * charge_ms_;
	return EquipmentEvent::CHARGE;
}
