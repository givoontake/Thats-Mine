#pragma once
#include <cstdint>
#include <optional>
#include <vector>
#include "map_geometry.h"
#include "terrain_types.h"

class MapFileReader;

// 게임 맵의 랜드스케이프(높이 격자 충돌 조각)를 갖고 선분 막힘, 땅 높이, 캡슐 쓸기·겹침을 답한다.
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
	// 월드 상자 min~max에 걸치는 칸마다 칸의 삼각형 2개(월드 좌표, 구멍 칸 제외)를 visit에 넘긴다. visit이 true를 돌려주면 멈추고 true를 돌려준다.
	template <typename Visit>
	bool VisitTriangles(const DirectX::XMFLOAT3& min, const DirectX::XMFLOAT3& max, Visit visit) const;

public:
	// 랜드스케이프 파일의 머리 뒤 충돌 조각 부분을 읽는다.
	bool Load(MapFileReader& reader);
	bool IsLoaded() const { return is_loaded_; }
	// 선분이 랜드스케이프에 닿는지 본다.
	bool IsSegmentBlockedByLandscape(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end) const;
	// (x, y) 지점의 랜드스케이프 높이. 랜드스케이프 밖이거나 구멍이면 값이 없다.
	std::optional<float> GetGroundHeight(float x, float y) const;
	// 세로로 선 캡슐을 중심 start에서 end까지 옮길 때 랜드스케이프에 처음 닿는 곳. 반지름과 반 높이가 0이면 선분 검사다.
	// 삼각형은 양쪽 면 모두 막고, 닿은 면 법선은 늘 위쪽 면 법선이다.
	MoveHit SweepCapsule(const MoveVector& start, const MoveVector& end, float radius, float half_height, bool is_ignoring_moving_out) const;
	// 세로로 선 캡슐(center, 반지름, 반 높이)이 랜드스케이프 면과 겹치거나 아래 끝이 땅 밑에 있는지 본다.
	bool IsCapsuleOverlapping(const MoveVector& center, float radius, float half_height) const;
};
