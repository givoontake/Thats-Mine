#pragma once
#include <algorithm>
#include <cstddef>
#include <limits>
#include <numeric>
#include <random>
#include "random_seed.h"
#include <vector>

// 미첼 최적 후보 방식으로 고를 때 한 번에 뽑아 보는 후보 수. 클수록 고르게 퍼지고, 1이면 완전 무작위다.
constexpr int SPREAD_SAMPLE_COUNT = 10;
// 고른 후보가 이 횟수만큼 연달아 최소 거리를 못 지키면 최소 거리를 SPREAD_MIN_DISTANCE_SHRINK배로 줄여 개수를 채운다.
constexpr int SPREAD_RETRY_COUNT = 20;
constexpr float SPREAD_MIN_DISTANCE_SHRINK = 0.9f;

// candidates 중 count개를 미첼 최적 후보 방식으로 고르게 퍼뜨려 고르고, 고른 순서대로 후보 번호를 돌려준다. 위치 타입은 x·y(cm)만 쓴다.
// 쓰지 않은 후보를 SPREAD_SAMPLE_COUNT개 뽑아 이미 고른 후보까지 가장 가까운 수평 거리가 가장 먼 후보를 고르고, 그 거리가 min_distance보다 가까우면 고르지 않고 다시 뽑는다.
template <typename Location>
std::vector<std::size_t> SelectSpreadIndices(const std::vector<Location>& candidates, int count, float min_distance)
{
	std::mt19937 random(CreateRandomSeed());
	std::vector<std::size_t> selected;
	std::vector<std::size_t> unused(candidates.size());
	std::iota(unused.begin(), unused.end(), std::size_t{ 0 });
	int failure_count = 0;
	while (static_cast<int>(selected.size()) < count && !unused.empty()) {
		std::size_t best = 0;
		float best_distance_sq = -1.0f;
		for (int sample = 0; sample < SPREAD_SAMPLE_COUNT; ++sample) {
			const std::size_t index = std::uniform_int_distribution<std::size_t>(0, unused.size() - 1)(random);
			const Location& location = candidates[unused[index]];
			float nearest_sq = std::numeric_limits<float>::max();
			for (const std::size_t chosen : selected) {
				const float dx = candidates[chosen].x - location.x;
				const float dy = candidates[chosen].y - location.y;
				nearest_sq = std::min(nearest_sq, dx * dx + dy * dy);
			}
			if (nearest_sq > best_distance_sq) {
				best = index;
				best_distance_sq = nearest_sq;
			}
		}
		if (best_distance_sq < min_distance * min_distance) {
			if (++failure_count >= SPREAD_RETRY_COUNT) {
				min_distance *= SPREAD_MIN_DISTANCE_SHRINK;
				failure_count = 0;
			}
			continue;
		}
		failure_count = 0;
		selected.push_back(unused[best]);
		unused[best] = unused.back();
		unused.pop_back();
	}
	return selected;
}
