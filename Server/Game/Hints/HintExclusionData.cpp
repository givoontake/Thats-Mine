#include "HintExclusionData.h"
#include "DirectionHintData.h"
#include "HintObjectTree.h"
#include "MapData.h"
#include "MapRegionData.h"
#include "ViewHintData.h"
#include "settings.h"
#include "treasure_placement.h"

bool HintExclusionData::Build(const MapData& map, const MapRegionData& regions, const DirectionHintData& directions, const ViewHintData& views, const HintObjectTree& tree)
{
	is_loaded_ = false;
	points_.clear();
	direction_true_counts_.assign(regions.GetRegions().size(), {});
	view_true_counts_.assign(regions.GetRegions().size(), 0);
	nearest_counts_ = {};
	// 보물은 맵 경계에서 TREASURE_EDGE_MARGIN 안쪽에만 놓으므로 같은 범위를 훑는다.
	const std::vector<std::uint32_t> no_placed_objects;
	for (float x = MAP_MIN[0] + TREASURE_EDGE_MARGIN; x <= MAP_MAX[0] - TREASURE_EDGE_MARGIN; x += HINT_EXCLUSION_GRID_STEP) {
		for (float y = MAP_MIN[1] + TREASURE_EDGE_MARGIN; y <= MAP_MAX[1] - TREASURE_EDGE_MARGIN; y += HINT_EXCLUSION_GRID_STEP) {
			if (!FindTreasureGround(map, regions, x, y, no_placed_objects)) continue;
			points_.push_back({ x, y });
			for (const HINT_ENTRY& hint : directions.JudgeDirections(x, y)) {
				if (hint.is_true) ++direction_true_counts_[hint.target][hint.detail];
			}
			for (const HINT_ENTRY& hint : views.JudgeViews(map, x, y)) {
				if (hint.is_true) ++view_true_counts_[hint.target];
			}
			const NearestHintKinds nearest = tree.FindNearestKinds(x, y);
			for (std::size_t rank = 0; rank < nearest.count; ++rank) ++nearest_counts_[rank][static_cast<std::size_t>(nearest.kinds[rank].kind)];
		}
	}
	is_loaded_ = !points_.empty();
	return is_loaded_;
}

float HintExclusionData::GetExclusion(const HINT_ENTRY& hint) const
{
	if (!is_loaded_) return 0.0f;
	const float point_count = static_cast<float>(points_.size());
	if (hint.kind == static_cast<std::uint8_t>(HintKind::NEAREST_OBJECT)) return 1.0f - nearest_counts_[hint.detail][hint.target] / point_count;
	std::uint32_t true_count = 0;
	if (hint.kind == static_cast<std::uint8_t>(HintKind::DIRECTION)) true_count = direction_true_counts_[hint.target][hint.detail];
	else if (hint.kind == static_cast<std::uint8_t>(HintKind::VIEW)) true_count = view_true_counts_[hint.target];
	else return 0.0f;
	// 있다·보인다 힌트는 참인 지점이, 없다·안 보인다 힌트는 참이 아닌 지점이 힌트와 맞는 땅이다.
	const float same_count = hint.is_true ? true_count : point_count - true_count;
	return 1.0f - same_count / point_count;
}

float HintExclusionData::GetNearestTowerExclusion(TowerKind kind, const std::vector<TowerPlacement>& tower_placements, const std::vector<DirectX::XMFLOAT3>& tower_candidates) const
{
	if (!is_loaded_) return 0.0f;
	std::size_t same_count = 0;
	for (const DirectX::XMFLOAT2& point : points_) {
		const TowerPlacement* nearest = FindNearestTower(tower_placements, tower_candidates, point.x, point.y);
		if (nearest && nearest->kind == kind) ++same_count;
	}
	return 1.0f - static_cast<float>(same_count) / points_.size();
}
