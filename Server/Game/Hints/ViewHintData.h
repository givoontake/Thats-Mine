#pragma once
#include <string>
#include <vector>
#include "hint_types.h"

class MapData;
class MapRegionData;

// 보임 힌트("보물이 묻힌 자리가 X에서 보인다/보이지 않는다")의 영역별 판정 위치를 서버 시작 때 읽어 둔다.
// 판정 위치 중 하나라도, 보물 자리 지면 PLACE_VIEW_END_MARGIN 앞까지 그은 선이 랜드스케이프와 장소 시야를 막는 배치물에 막히지 않으면 보인다.
// 읽은 뒤에는 바뀌지 않으므로 여러 방 스레드가 잠금 없이 같이 읽는다.
class ViewHintData
{
	std::vector<ViewRegion> regions_;
	bool is_loaded_ = false;

public:
	// 판정 위치 파일을 읽어 영역별로 묶는다. 영역 이름은 맵 영역에 있는 이름이어야 한다.
	bool Load(const std::string& file_path, const MapRegionData& regions);
	bool IsLoaded() const { return is_loaded_; }
	// 지면 (x, y) 자리가 기준 영역마다 보이는지 판정한다. 기준 영역 순서로 하나씩 돌려준다.
	std::vector<HINT_ENTRY> JudgeViews(const MapData& map, float x, float y) const;
};
