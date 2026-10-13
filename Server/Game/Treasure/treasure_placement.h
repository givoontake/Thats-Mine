#pragma once
#include <cstdint>
#include <optional>
#include <vector>

class MapData;
class MapRegionData;

// (x, y)에 최종 보물을 놓을 수 있으면 보물 바닥 높이(cm)를, 놓을 수 없으면 값 없음을 돌려준다.
// 보물을 놓지 않는 영역 밖이고, 보물 둘레 사각형 안의 땅이 모두 있고, 보물을 감싼 구가 배치물(건물·나무·바위와 placed_objects의 탑)과 겹치지 않아야 한다.
// 바닥은 그 사각형 안 가장 낮은 땅에 맞춰 뜨지 않게 한다.
std::optional<float> FindTreasureGround(const MapData& map, const MapRegionData& regions, float x, float y, const std::vector<std::uint32_t>& placed_objects);
