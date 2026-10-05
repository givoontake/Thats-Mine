#pragma once
#include <array>
#include <cstdint>
#include <random>
#include "platform_coordinates.h"
#include "settings.h"

// 발판 자리마다 놓을지 여부. 순서는 PLATFORM_COORDINATES와 같다.
using PlatformLayout = std::array<bool, PLATFORM_COORDINATE_COUNT>;

// 게임마다 발판 배치를 정한다. 서버가 게임 시작 때 정해 보내고, 로컬 플레이에서는 클라이언트가 직접 정한다.
// 시작·모서리·도착 발판과 빈자리 바로 다음 발판은 반드시 놓아 빈자리가 연달아 생기지 않게 하고, 나머지는 PLATFORM_SPAWN_PROBABILITY 확률로 놓는다.
// 서버와 클라이언트가 같이 쓰므로 헤더에만 둔다.
inline PlatformLayout GeneratePlatformLayout(std::uint32_t seed)
{
	std::mt19937 random(seed);
	std::bernoulli_distribution spawn(PLATFORM_SPAWN_PROBABILITY);
	PlatformLayout layout{};
	bool is_previous_spawned = true;
	for (int index = 0; index < PLATFORM_COORDINATE_COUNT; ++index) {
		const bool is_fixed = PLATFORM_COORDINATES[index].type != PlatformType::NORMAL;
		layout[index] = is_fixed || !is_previous_spawned || spawn(random);
		is_previous_spawned = layout[index];
	}
	return layout;
}
