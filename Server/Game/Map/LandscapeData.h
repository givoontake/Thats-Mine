#pragma once
#include <cstdint>
#include <optional>
#include <vector>
#include "terrain_types.h"

class MapFileReader;

// 게임 맵의 랜드스케이프(높이 격자 충돌 조각)를 갖고 선분 막힘과 땅 높이를 답한다.
// 읽은 뒤에는 바뀌지 않으므로 여러 방 스레드가 잠금 없이 같이 읽는다.
class LandscapeData
{
	std::vector<TerrainHeightfield> heightfields_;
	// 땅 높이를 찾을 조각을 바로 고르는 칸. 모든 조각 범위를 덮는 사각형을 가장 작은 조각 한 변 길이로 나누고,
	// 칸마다 범위가 그 칸에 걸치는 조각 번호를 heightfields_ 순서대로 둔다.
	DirectX::XMFLOAT2 grid_min_{};
	DirectX::XMFLOAT2 grid_max_{};
	float grid_cell_size_ = 0.0f;
	int grid_columns_ = 0;
	int grid_rows_ = 0;
	std::vector<std::vector<std::uint32_t>> grid_cells_;
	bool is_loaded_ = false;

	void BuildGrid();

public:
	// 랜드스케이프 파일의 머리 뒤 충돌 조각 부분을 읽는다.
	bool Load(MapFileReader& reader);
	bool IsLoaded() const { return is_loaded_; }
	// 선분이 랜드스케이프에 닿는지 본다.
	bool IsSegmentBlockedByLandscape(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end) const;
	// (x, y) 지점의 랜드스케이프 높이. 랜드스케이프 밖이거나 구멍이면 값이 없다.
	std::optional<float> GetGroundHeight(float x, float y) const;
};
