#pragma once
#include <vector>
#include "hint_types.h"

class MapRegionData;

// 방향 힌트("보물은 X보다 동쪽에 있다/없다")의 기준 사각형을 서버 시작 때 맵 영역에서 만들어 둔다.
// 기준 사각형은 영역 스플라인 조종점의 최소·최대 좌표에서 HINT_DIRECTION_MARGIN만큼 넓힌 축 정렬 사각형이다.
// 방위는 지도 기준으로 북쪽이 x가 작은 쪽, 동쪽이 y가 작은 쪽이다.
// 만든 뒤에는 바뀌지 않으므로 여러 방 스레드가 잠금 없이 같이 읽는다.
class DirectionHintData
{
	std::vector<DirectionArea> areas_;
	bool is_loaded_ = false;

public:
	// 보물 제외 전용 영역을 뺀 모든 맵 영역으로 기준 사각형을 만든다.
	bool Build(const MapRegionData& regions);
	bool IsLoaded() const { return is_loaded_; }
	// (x, y)가 기준 영역마다 네 방위 쪽에 있는지 판정한다. 기준 영역 순서로, 영역마다 HintDirection 순서의 힌트 네 개를 돌려준다.
	std::vector<Hint> JudgeDirections(float x, float y) const;
};
