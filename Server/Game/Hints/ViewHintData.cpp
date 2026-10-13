#include <algorithm>
#include <cmath>
#include <fstream>
#include <optional>
#include <sstream>
#include "ViewHintData.h"
#include "MapData.h"
#include "MapRegionData.h"

// 줄: x y z 영역이름. 이름은 줄 끝까지(띄어쓰기 포함)다.
bool ViewHintData::Load(const std::string& file_path, const MapRegionData& regions)
{
	regions_.clear();
	is_loaded_ = false;
	std::ifstream file(file_path);
	if (!file) return false;

	const std::vector<MapRegion>& map_regions = regions.GetRegions();
	std::string line;
	while (std::getline(file, line))
	{
		if (line.empty() || line[0] == '#') continue;

		std::istringstream stream(line);
		DirectX::XMFLOAT3 eye{};
		std::string name;
		stream >> eye.x >> eye.y >> eye.z;
		std::getline(stream >> std::ws, name);
		if (!stream) return false;
		const auto map_region = std::find_if(map_regions.begin(), map_regions.end(), [&](const MapRegion& region) { return region.name == name; });
		if (map_region == map_regions.end()) return false;
		const std::uint8_t region_index = static_cast<std::uint8_t>(map_region - map_regions.begin());
		auto view_region = std::find_if(regions_.begin(), regions_.end(), [&](const ViewRegion& region) { return region.region == region_index; });
		if (view_region == regions_.end()) view_region = regions_.insert(regions_.end(), ViewRegion{ region_index, {} });
		view_region->eyes.push_back(eye);
	}
	is_loaded_ = !regions_.empty();
	return is_loaded_;
}

std::vector<HINT_ENTRY> ViewHintData::JudgeViews(const MapData& map, float x, float y) const
{
	std::vector<HINT_ENTRY> hints;
	const std::optional<float> ground = map.GetLandscape().GetGroundHeight(x, y);
	if (!ground) return hints;
	// 탑은 장소 시야를 막지 않으므로 놓인 것이 없다고 본다.
	const std::vector<std::uint32_t> no_placed_objects;
	hints.reserve(regions_.size());
	for (const ViewRegion& region : regions_) {
		bool is_visible = false;
		for (const DirectX::XMFLOAT3& eye : region.eyes) {
			const float dx = x - eye.x, dy = y - eye.y, dz = *ground - eye.z;
			const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
			const float scale = length > PLACE_VIEW_END_MARGIN ? (length - PLACE_VIEW_END_MARGIN) / length : 0.0f;
			const DirectX::XMFLOAT3 end{ eye.x + dx * scale, eye.y + dy * scale, eye.z + dz * scale };
			if (!map.GetLandscape().IsSegmentBlockedByLandscape(eye, end) && !map.GetCollisionObjects().IsSegmentBlockedByObjects(eye, end, TERRAIN_BLOCKS_PLACE_VIEW, no_placed_objects)) {
				is_visible = true;
				break;
			}
		}
		hints.push_back({ static_cast<std::uint8_t>(HintKind::VIEW), region.region, 0, is_visible, 0 });
	}
	return hints;
}
