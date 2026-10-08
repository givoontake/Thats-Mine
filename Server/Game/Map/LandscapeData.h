#pragma once
#include <optional>
#include <vector>
#include "terrain_types.h"

class MapFileReader;

// 게임 맵의 랜드스케이프(높이 격자 충돌 조각)를 갖고 선분 막힘과 땅 높이를 답한다.
// 읽은 뒤에는 바뀌지 않으므로 여러 방 스레드가 잠금 없이 같이 읽는다.
class LandscapeData
{
	std::vector<TerrainHeightfield> heightfields_;
	bool is_loaded_ = false;

public:
	// 맵 파일의 랜드스케이프 충돌 조각 부분을 읽는다.
	bool Load(MapFileReader& reader);
	bool IsLoaded() const { return is_loaded_; }
	// 선분이 랜드스케이프에 닿는지 본다.
	bool IsSegmentBlockedByLandscape(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end) const;
	// (x, y) 지점의 랜드스케이프 높이. 랜드스케이프 밖이거나 구멍이면 값이 없다.
	std::optional<float> GetGroundHeight(float x, float y) const;
};
