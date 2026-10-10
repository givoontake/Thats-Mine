#pragma once
#include <cstddef>
#include <vector>
#include "hint_types.h"

class MapData;

// 힌트 물체(바위·나무·대나무·데이지·버섯)의 지도 위치로 만든 2차원 k-d 트리. 서버 시작 때 맵 데이터에서 한 번 만든다.
// 노드를 따로 두지 않는다. 점 배열의 구간 [시작, 끝)에서 가운데 점이 그 구간의 기준선이고, 깊이가 짝수면 x, 홀수면 y로 나눈다.
// 가운데 점은 기준선으로 그 자리에 두고, 왼쪽 자식은 [시작, 가운데), 오른쪽 자식은 [가운데 + 1, 끝)이다.
// 만든 뒤에는 바뀌지 않으므로 여러 방 스레드가 잠금 없이 같이 찾는다.
class HintObjectTree
{
	std::vector<HintPoint> points_;
	bool is_loaded_ = false;

	void BuildRange(std::size_t begin, std::size_t end, int depth);
	// result는 재귀 전체가 같이 채우는 순위 목록이다. 아래 구간을 볼지 정할 때 지금까지의 세 번째 거리가 필요해서 넘겨 쓴다.
	void SearchRange(std::size_t begin, std::size_t end, int depth, float x, float y, NearestHintKinds& result) const;

public:
	// 맵 데이터에서 힌트 종류 메시의 물체 중 맵 범위 안의 것을 모아 트리를 만든다. 대응표의 메시가 맵 데이터에 없으면 실패한다.
	bool Build(const MapData& map);
	bool IsLoaded() const { return is_loaded_; }
	// (x, y)에서 가까운 종류를 서로 다른 종류로 최대 HINT_NEAREST_KIND_COUNT개, 가까운 순서로 돌려준다. 거리는 수평 거리다.
	NearestHintKinds FindNearestKinds(float x, float y) const;
};
