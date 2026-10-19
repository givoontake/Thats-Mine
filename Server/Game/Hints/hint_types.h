#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <variant>
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

// 방향 힌트 기준 영역 하나. region은 맵 영역 순서 번호이고, 사각형은 지도 좌표(cm)다.
struct DirectionArea {
	std::uint8_t region;
	float min_x;
	float max_x;
	float min_y;
	float max_y;
};

// 방향 힌트 하나. area는 맵 영역 순서 번호, direction은 HintDirection이고, is_true면 보물이 그 영역보다 그 방위에 있다.
struct DirectionHint {
	std::uint8_t area;
	std::uint8_t direction;
	bool is_true;
};

// 개수 힌트 하나. 보물 바닥에서 HINT_OBJECT_COUNT_RADII의 radius번째 반경 안에 있는 object_kind(HintObjectKind) 물체 수(최대 255)다.
struct CountHint {
	std::uint8_t object_kind;
	std::uint8_t radius;
	std::uint8_t count;
};

// 이번 게임의 힌트 하나. 힌트 번호는 방의 힌트 목록 순서다.
using Hint = std::variant<DirectionHint, CountHint>;
