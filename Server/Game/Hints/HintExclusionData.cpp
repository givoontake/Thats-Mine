#include <algorithm>
#include <limits>
#include <optional>
#include "HintExclusionData.h"
#include "MapData.h"
#include "settings.h"

bool HintExclusionData::Build(const MapData& map)
{
	is_loaded_ = false;
	// 칸마다 칸 안을 훑어 땅 높이 범위를 잰다. 땅이 없는 칸은 범위를 비워 둔다(최솟값이 최댓값보다 크다).
	for (int cell = 0; cell < BLOCK_GRID_SIZE * BLOCK_GRID_SIZE; ++cell) {
		const float x0 = MAP_MIN[0] + (cell / BLOCK_GRID_SIZE) * BLOCK_CELL_SIZE_X;
		const float y1 = MAP_MAX[1] - (cell % BLOCK_GRID_SIZE) * BLOCK_CELL_SIZE_Y;
		DirectX::XMFLOAT2 range{ std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest() };
		for (float x = x0; x <= x0 + BLOCK_CELL_SIZE_X; x += MAGNIFIER_HEIGHT_SAMPLE_STEP) {
			for (float y = y1 - BLOCK_CELL_SIZE_Y; y <= y1; y += MAGNIFIER_HEIGHT_SAMPLE_STEP) {
				const std::optional<float> height = map.GetLandscape().GetGroundHeight(x, y);
				if (!height) continue;
				range.x = std::min(range.x, *height);
				range.y = std::max(range.y, *height);
			}
		}
		cell_height_ranges_[cell] = range;
	}
	is_loaded_ = true;
	return is_loaded_;
}
