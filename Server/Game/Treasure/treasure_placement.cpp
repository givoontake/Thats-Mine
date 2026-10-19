#include <algorithm>
#include "treasure_placement.h"
#include "MapData.h"
#include "MapRegionData.h"
#include "settings.h"

std::optional<float> FindTreasureGround(const MapData& map, const MapRegionData& regions, float x, float y)
{
	if (regions.IsTreasureExcluded(x, y)) return std::nullopt;
	const LandscapeData& landscape = map.GetLandscape();
	std::optional<float> ground = landscape.GetGroundHeight(x, y);
	for (int corner = 0; corner < 4 && ground; ++corner) {
		const std::optional<float> corner_ground = landscape.GetGroundHeight(x + ((corner & 1) ? TREASURE_RADIUS : -TREASURE_RADIUS), y + ((corner & 2) ? TREASURE_RADIUS : -TREASURE_RADIUS));
		ground = corner_ground ? std::optional<float>(std::min(*ground, *corner_ground)) : std::nullopt;
	}
	if (!ground) return std::nullopt;
	if (map.GetCollisionObjects().IsCapsuleOverlappingObjects({ x, y, *ground + TREASURE_HALF_HEIGHT }, TREASURE_RADIUS, TREASURE_HALF_HEIGHT, TERRAIN_BLOCKS_PAWN)) return std::nullopt;
	return ground;
}
