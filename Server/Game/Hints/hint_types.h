#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <DirectXMath.h>
#include "game_packets.h"
#include "settings.h"

// 힌트 물체 트리의 점. 물체 바닥 위치의 지도 좌표·높이(cm)와 종류다.
struct HintPoint {
	float x;
	float y;
	float z;
	HintObjectKind kind;
};

// 물체 종류마다 어떤 범위 안에 있는 물체 수. DAISY·MUSHROOM 칸은 모든 색을 합친 수다.
using HintKindCounts = std::array<std::uint32_t, static_cast<std::size_t>(HintObjectKind::COUNT)>;

// 색 데이지·버섯을 DAISY·MUSHROOM으로 바꾼 종류. 나머지는 그대로다.
constexpr HintObjectKind GetBaseHintObjectKind(HintObjectKind kind)
{
	if (kind >= HintObjectKind::DAISY_RED && kind <= HintObjectKind::DAISY_WHITE) return HintObjectKind::DAISY;
	if (kind >= HintObjectKind::MUSHROOM_RED && kind <= HintObjectKind::MUSHROOM_BLUE) return HintObjectKind::MUSHROOM;
	return kind;
}

// 가까운 종류 하나와 그 종류 물체 중 가장 가까운 것까지의 거리(cm)
struct HintKindDistance {
	HintObjectKind kind;
	float distance;
};

// 가까운 종류 순위. kinds의 앞 count개가 가까운 순서이고, 종류는 서로 다르다.
struct NearestHintKinds {
	std::array<HintKindDistance, HINT_NEAREST_KIND_COUNT> kinds;
	std::size_t count;
};

// 방향 힌트 기준 영역 하나. region은 맵 영역 순서 번호이고, 사각형은 지도 좌표(cm)다.
struct DirectionArea {
	std::uint8_t region;
	float min_x;
	float max_x;
	float min_y;
	float max_y;
};

// 보임 힌트 기준 영역 하나. region은 맵 영역 순서 번호이고, eyes는 판정 위치(눈 위치, cm)다.
struct ViewRegion {
	std::uint8_t region;
	std::vector<DirectX::XMFLOAT3> eyes;
};

// 기절한 플레이어가 떨어뜨린 힌트 하나. hint는 힌트 목록 번호, location은 떨어뜨린 플레이어의 캡슐 중심(cm)이다.
struct HintDrop {
	std::uint32_t hint;
	DirectX::XMFLOAT3 location;
	bool is_picked;
};
