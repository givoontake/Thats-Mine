#include <algorithm>
#include <limits>
#include "DirectionHintData.h"
#include "MapRegionData.h"

// 보물 제외에만 쓰는 영역. 장소 이름이 아니라서 방향 힌트 기준으로 쓰지 않는다.
static constexpr const char* DIRECTION_EXCLUDED_REGION = "Asian Village Cliff";

bool DirectionHintData::Build(const MapRegionData& regions)
{
	constexpr float INF = std::numeric_limits<float>::infinity();
	is_loaded_ = false;
	areas_.clear();
	const std::vector<MapRegion>& map_regions = regions.GetRegions();
	for (std::size_t index = 0; index < map_regions.size(); ++index) {
		const MapRegion& region = map_regions[index];
		if (region.name == DIRECTION_EXCLUDED_REGION) continue;
		DirectionArea area{ static_cast<std::uint8_t>(index), INF, -INF, INF, -INF };
		for (const DirectX::XMFLOAT2& point : region.points) {
			area.min_x = std::min(area.min_x, point.x);
			area.max_x = std::max(area.max_x, point.x);
			area.min_y = std::min(area.min_y, point.y);
			area.max_y = std::max(area.max_y, point.y);
		}
		area.min_x -= HINT_DIRECTION_MARGIN;
		area.max_x += HINT_DIRECTION_MARGIN;
		area.min_y -= HINT_DIRECTION_MARGIN;
		area.max_y += HINT_DIRECTION_MARGIN;
		areas_.push_back(area);
	}
	is_loaded_ = !areas_.empty();
	return is_loaded_;
}

std::optional<float> DirectionHintData::FindBoundary(std::uint8_t region, HintDirection direction) const
{
	const auto area = std::find_if(areas_.begin(), areas_.end(), [&](const DirectionArea& candidate) { return candidate.region == region; });
	if (area == areas_.end()) return std::nullopt;
	switch (direction) {
	case HintDirection::NORTH: return area->min_x;
	case HintDirection::SOUTH: return area->max_x;
	case HintDirection::EAST: return area->min_y;
	case HintDirection::WEST: return area->max_y;
	default: return std::nullopt;
	}
}

std::vector<HINT_ENTRY> DirectionHintData::JudgeDirections(float x, float y) const
{
	constexpr int DIRECTION_COUNT = static_cast<int>(HintDirection::COUNT);
	std::vector<HINT_ENTRY> hints;
	hints.reserve(areas_.size() * DIRECTION_COUNT);
	for (const DirectionArea& area : areas_) {
		// HintDirection 순서(북, 동, 남, 서)
		const bool is_in_direction[DIRECTION_COUNT] = { x < area.min_x, y < area.min_y, x > area.max_x, y > area.max_y };
		for (int direction = 0; direction < DIRECTION_COUNT; ++direction) {
			hints.push_back({ static_cast<std::uint8_t>(HintKind::DIRECTION), area.region, static_cast<std::uint8_t>(direction), is_in_direction[direction], 0 });
		}
	}
	return hints;
}
