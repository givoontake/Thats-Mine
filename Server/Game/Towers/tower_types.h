#pragma once
#include <cstddef>
#include <limits>
#include <vector>
#include <DirectXMath.h>
#include "game_packets.h"

// 이번 게임에 놓은 탑 하나. 후보 번호는 탑 후보 자리 목록의 번호다.
struct TowerPlacement {
	std::size_t candidate_index;
	TowerKind kind;
};

// 놓인 탑 중 (x, y, z)에서 탑 바닥 가운데까지 거리로 가장 가까운 탑. 놓인 탑이 없으면 nullptr이다.
inline const TowerPlacement* FindNearestTower(const std::vector<TowerPlacement>& tower_placements, const std::vector<DirectX::XMFLOAT3>& tower_candidates, float x, float y, float z)
{
	const TowerPlacement* nearest = nullptr;
	float nearest_distance_sq = std::numeric_limits<float>::max();
	for (const TowerPlacement& placement : tower_placements) {
		const DirectX::XMFLOAT3& location = tower_candidates[placement.candidate_index];
		const float dx = location.x - x;
		const float dy = location.y - y;
		const float dz = location.z - z;
		const float distance_sq = dx * dx + dy * dy + dz * dz;
		if (distance_sq < nearest_distance_sq) {
			nearest_distance_sq = distance_sq;
			nearest = &placement;
		}
	}
	return nearest;
}
