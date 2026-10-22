#pragma once
#include <cstdint>
#include <vector>
#include "hint_types.h"

class MapData;

// 힌트 물체(바위·나무·대나무·색 데이지·색 버섯)의 지도 위치를 HINT_OBJECT_GRID_CELL_SIZE 크기 칸으로 나눈 격자. 서버 시작 때 맵 데이터에서 한 번 만든다.
// 점은 칸 순서로 한 배열에 이어 담고, 칸 c의 점은 points_[cell_starts_[c]]부터 points_[cell_starts_[c + 1]] 전까지다.
// 칸은 지도 x 방향으로 columns_개, y 방향으로 rows_개이고, 칸 번호는 행 × columns_ + 열이다. 지도 끝 칸은 남는 폭만큼 좁다.
// 만든 뒤에는 바뀌지 않으므로 여러 방 스레드가 잠금 없이 같이 찾는다.
class HintObjectGrid
{
	std::vector<HintPoint> points_;
	std::vector<std::uint32_t> cell_starts_;
	int columns_ = 0;
	int rows_ = 0;
	bool is_loaded_ = false;

public:
	// 맵 데이터에서 힌트 종류 메시의 물체 중 맵 범위 안의 것을 모아 격자를 만든다. 대응표의 힌트 종류 중 맵 데이터에서 이름을 하나도 찾지 못한 종류가 있으면 실패한다.
	bool Build(const MapData& map);
	bool IsLoaded() const { return is_loaded_; }
	// (x, y, z)에서 거리 radius(cm) 안에 있는 물체 수를 종류마다 센다. 거리는 높이까지 넣은 3차원 거리이고, 반경을 감싸는 사각형이 걸치는 칸만 본다.
	HintKindCounts CountKinds(float x, float y, float z, float radius) const;
};
