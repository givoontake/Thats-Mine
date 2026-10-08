#pragma once
#include <string>
#include <vector>
#include "region_types.h"

// 맵 영역(장소 이름과 범위)을 서버 시작 때 읽어 둔다.
// 읽은 뒤에는 바뀌지 않으므로 여러 방 스레드가 잠금 없이 같이 읽는다.
class MapRegionData
{
	std::vector<MapRegion> regions_;
	bool is_loaded_ = false;

public:
	bool Load(const std::string& region_path);
	bool IsLoaded() const { return is_loaded_; }
	const std::vector<MapRegion>& GetRegions() const { return regions_; }
	// (x, y)가 보물을 놓지 않는 영역 안에 있는지 본다.
	bool IsTreasureExcluded(float x, float y) const;
};
