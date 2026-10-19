#include <algorithm>
#include <iterator>
#include <limits>
#include "DirectionHintData.h"
#include "MapRegionData.h"

// 방향 힌트 기준으로 쓰지 않는 영역. Asian Village Cliff는 보물 제외에만 쓰는 영역이고, Daisy Meadow는 보물을 놓지 않는 꽃밭이라 힌트에서도 뺀다.
static constexpr const char* DIRECTION_EXCLUDED_REGIONS[] = { "Asian Village Cliff", "Daisy Meadow" };

bool DirectionHintData::Build(const MapRegionData& regions)
{
	constexpr float INF = std::numeric_limits<float>::infinity();
	is_loaded_ = false;
	areas_.clear();
	const std::vector<MapRegion>& map_regions = regions.GetRegions();
	for (std::size_t index = 0; index < map_regions.size(); ++index) {
		const MapRegion& region = map_regions[index];
		if (std::find(std::begin(DIRECTION_EXCLUDED_REGIONS), std::end(DIRECTION_EXCLUDED_REGIONS), region.name) != std::end(DIRECTION_EXCLUDED_REGIONS)) continue;
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

std::vector<Hint> DirectionHintData::JudgeDirections(float x, float y) const
{
	constexpr int DIRECTION_COUNT = static_cast<int>(HintDirection::COUNT);
	std::vector<Hint> hints;
	hints.reserve(areas_.size() * DIRECTION_COUNT);
	for (const DirectionArea& area : areas_) {
		// HintDirection 순서(북, 동, 남, 서)
		const bool is_in_direction[DIRECTION_COUNT] = { x < area.min_x, y < area.min_y, x > area.max_x, y > area.max_y };
		for (int direction = 0; direction < DIRECTION_COUNT; ++direction) {
			hints.push_back(DirectionHint{ area.region, static_cast<std::uint8_t>(direction), is_in_direction[direction] });
		}
	}
	return hints;
}
