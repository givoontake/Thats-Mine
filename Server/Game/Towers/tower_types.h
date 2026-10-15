#pragma once
#include <cstddef>
#include "game_packets.h"

// 이번 게임에 놓은 탑 하나. 후보 번호는 탑 후보 자리 목록의 번호다.
struct TowerPlacement {
	std::size_t candidate_index;
	TowerKind kind;
};
