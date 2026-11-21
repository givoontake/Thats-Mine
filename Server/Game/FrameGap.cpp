#include "FrameGap.h"
#include "movement_settings.h"

void FrameGap::Start(std::uint32_t spawn_tick)
{
	spawn_tick_ = spawn_tick;
	has_gap_ = false;
	complete_number_ = -1;
}

void FrameGap::MarkComplete(std::uint32_t frame_number, std::uint32_t tick)
{
	complete_number_ = frame_number;
	if (has_gap_) return;
	gap_ = GetServerCount(tick) - frame_number + FRAME_GAP_ALLOWANCE_TICKS;
	has_gap_ = true;
}

void FrameGap::BeginTick(std::uint32_t tick)
{
	if (has_gap_ && complete_number_ > GetProcessNumber(tick)) --gap_;
}

void FrameGap::SettleTick(std::uint32_t tick)
{
	if (has_gap_ && complete_number_ < GetProcessNumber(tick)) ++gap_;
}
