#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include <DirectXMath.h>
#include "block_pieces.h"
#include "hint_types.h"
#include "tower_types.h"

class DirectionHintData;
class HintObjectTree;
class MapData;
class MapRegionData;
class ViewHintData;

// 힌트 배제 영역(보물 가능 땅 중 그 힌트가 틀린 땅의 비율)을 재는 데이터.
// 서버 시작 때 보물 가능 땅을 HINT_EXCLUSION_GRID_STEP 간격 격자 지점으로 모으고, 방향·보임·가까운 물체 힌트는 문장마다 참인 지점 수를 세어 둔다.
// 개수 힌트는 등급을 반경으로 정해 배제 영역을 재지 않는다.
// 돋보기 탐색 결과로 칸을 배제할 때 쓰려고 배제 판 칸마다 땅 높이 범위도 재어 둔다.
// 탑은 판마다 달라 보물 가능 땅 판정에서 뺀다. 만든 뒤에는 바뀌지 않으므로 여러 방 스레드가 잠금 없이 같이 읽는다.
class HintExclusionData
{
	std::vector<DirectX::XMFLOAT3> points_; // 보물 가능 땅 격자 지점(cm). z는 보물 바닥 높이다.
	std::vector<std::array<std::uint32_t, static_cast<std::size_t>(HintDirection::COUNT)>> direction_true_counts_; // [영역 번호][방위] 그 방위에 있는 지점 수
	std::vector<std::uint32_t> view_true_counts_; // [영역 번호] 그 영역에서 보이는 지점 수
	std::array<std::array<std::uint32_t, static_cast<std::size_t>(HintObjectKind::COUNT)>, HINT_NEAREST_KIND_COUNT> nearest_counts_{}; // [순위][종류] 그 순위가 그 종류인 지점 수
	// [칸 번호] 칸 안 땅 높이의 최솟값과 최댓값(cm). MAGNIFIER_HEIGHT_SAMPLE_STEP 간격으로 잰다.
	std::array<DirectX::XMFLOAT2, BLOCK_GRID_SIZE * BLOCK_GRID_SIZE> cell_height_ranges_{};
	bool is_loaded_ = false;

public:
	bool Build(const MapData& map, const MapRegionData& regions, const DirectionHintData& directions, const ViewHintData& views, const HintObjectTree& tree);
	bool IsLoaded() const { return is_loaded_; }
	// 방향·보임·가까운 물체 힌트의 배제 영역(0~1)
	float GetExclusion(const HINT_ENTRY& hint) const;
	// 가장 가까운 탑 힌트의 배제 영역(0~1). 탑 자리는 판마다 달라 그 판에 놓인 탑으로 지점마다 가장 가까운 탑 종류를 다시 구한다.
	float GetNearestTowerExclusion(TowerKind kind, const std::vector<TowerPlacement>& tower_placements, const std::vector<DirectX::XMFLOAT3>& tower_candidates) const;
	// 칸 안 땅 높이의 최솟값(x)과 최댓값(y)(cm)
	const DirectX::XMFLOAT2& GetCellHeightRange(int cell) const { return cell_height_ranges_[cell]; }
};
