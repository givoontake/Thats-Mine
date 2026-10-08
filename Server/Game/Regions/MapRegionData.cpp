#include "MapRegionData.h"
#include <fstream>
#include <sstream>
#include <utility>

// 점에서 +x 방향으로 뻗은 반직선이 다각형 변을 홀수 번 지나면 안쪽이다.
static bool IsInsidePolygon(const std::vector<DirectX::XMFLOAT2>& points, float x, float y)
{
	bool is_inside = false;
	for (std::size_t current = 0, previous = points.size() - 1; current < points.size(); previous = current++) {
		const DirectX::XMFLOAT2& a = points[current];
		const DirectX::XMFLOAT2& b = points[previous];
		if ((a.y > y) != (b.y > y) && x < (b.x - a.x) * (y - a.y) / (b.y - a.y) + a.x) is_inside = !is_inside;
	}
	return is_inside;
}

// 줄: 보물제외(0/1) 점수 x1 y1 x2 y2 ... 이름. 이름은 줄 끝까지(띄어쓰기 포함)다.
bool MapRegionData::Load(const std::string& region_path)
{
	regions_.clear();
	is_loaded_ = false;
	std::ifstream file(region_path);
	if (!file) return false;

	std::string line;
	while (std::getline(file, line))
	{
		if (line.empty() || line[0] == '#') continue;

		std::istringstream stream(line);
		int excluded = 0;
		int count = 0;
		stream >> excluded >> count;
		if (!stream || count < 3) return false;
		MapRegion region{};
		region.is_treasure_excluded = excluded != 0;
		region.points.resize(count);
		for (DirectX::XMFLOAT2& point : region.points) stream >> point.x >> point.y;
		std::getline(stream >> std::ws, region.name);
		if (!stream || region.name.empty()) return false;
		regions_.push_back(std::move(region));
	}
	is_loaded_ = !regions_.empty();
	return is_loaded_;
}

bool MapRegionData::IsTreasureExcluded(float x, float y) const
{
	for (const MapRegion& region : regions_) {
		if (region.is_treasure_excluded && IsInsidePolygon(region.points, x, y)) return true;
	}
	return false;
}
