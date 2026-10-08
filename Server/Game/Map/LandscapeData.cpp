#include <algorithm>
#include "LandscapeData.h"
#include "MapFileReader.h"
#include "map_geometry.h"

using namespace DirectX;

// 랜드스케이프 칸 플래그 중 구멍(충돌 없음)
constexpr std::uint8_t QUAD_NO_COLLISION = 128;

bool LandscapeData::Load(MapFileReader& reader)
{
	is_loaded_ = false;
	// 랜드스케이프 충돌 조각
	heightfields_.assign(reader.ReadCount(), {});
	for (TerrainHeightfield& heightfield : heightfields_) {
		const XMMATRIX local_to_world = reader.ReadTransform().ToMatrix();
		reader.ReadValue(heightfield.size);
		reader.ReadValue(heightfield.collision_scale);
		if (!reader.IsValid() || heightfield.size == 0 || heightfield.size > 4096) return false;
		heightfield.heights.resize(static_cast<size_t>(heightfield.size + 1) * (heightfield.size + 1));
		reader.Read(heightfield.heights.data(), heightfield.heights.size() * sizeof(std::uint16_t));
		heightfield.quad_flags.resize(static_cast<size_t>(heightfield.size) * heightfield.size);
		reader.Read(heightfield.quad_flags.data(), heightfield.quad_flags.size());
		if (!reader.IsValid()) return false;
		XMStoreFloat4x4(&heightfield.local_to_world, local_to_world);
		XMStoreFloat4x4(&heightfield.world_to_local, XMMatrixInverse(nullptr, local_to_world));
		const auto [low, high] = std::minmax_element(heightfield.heights.begin(), heightfield.heights.end());
		const float extent = heightfield.size * heightfield.collision_scale;
		const auto [world_min, world_max] = TransformBounds({ 0.0f, 0.0f, (*low - 32768.0f) / 128.0f }, { extent, extent, (*high - 32768.0f) / 128.0f }, local_to_world);
		heightfield.world_min = world_min;
		heightfield.world_max = world_max;
	}
	is_loaded_ = true;
	return true;
}

// 조각마다 선분을 조각 기준 좌표로 옮기고, 위에서 본 선분이 지나는 칸을 따라가며 칸의 삼각형 2개를 검사한다.
bool LandscapeData::IsSegmentBlockedByLandscape(const XMFLOAT3& start, const XMFLOAT3& end) const
{
	if (!is_loaded_) return false;
	for (const TerrainHeightfield& heightfield : heightfields_) {
		if (!SegmentHitsBox(start, end, heightfield.world_min, heightfield.world_max)) continue;
		const XMMATRIX world_to_local = XMLoadFloat4x4(&heightfield.world_to_local);
		const XMVECTOR local_start = XMVector3TransformCoord(XMLoadFloat3(&start), world_to_local);
		const XMVECTOR local_end = XMVector3TransformCoord(XMLoadFloat3(&end), world_to_local);
		const XMVECTOR segment = XMVectorSubtract(local_end, local_start);
		XMFLOAT3 from;
		XMFLOAT3 to;
		XMStoreFloat3(&from, local_start);
		XMStoreFloat3(&to, local_end);
		const float scale = heightfield.collision_scale;
		const float extent = heightfield.size * scale;
		const auto [enter, exit] = ClipSegment2D(from.x, from.y, to.x, to.y, 0.0f, 0.0f, extent, extent);
		if (enter > exit) continue;
		const int size = static_cast<int>(heightfield.size);
		const auto vertex = [&](int x, int y)
		{
			return XMVectorSet(x * scale, y * scale, (heightfield.heights[static_cast<size_t>(y) * (size + 1) + x] - 32768.0f) / 128.0f, 0.0f);
		};
		const bool is_hit = WalkCells((from.x + (to.x - from.x) * enter) / scale, (from.y + (to.y - from.y) * enter) / scale, (from.x + (to.x - from.x) * exit) / scale, (from.y + (to.y - from.y) * exit) / scale, size, size, [&](int x, int y)
		{
			if (heightfield.quad_flags[static_cast<size_t>(y) * size + x] & QUAD_NO_COLLISION) return false;
			const XMVECTOR p0 = vertex(x, y);
			const XMVECTOR p1 = vertex(x + 1, y);
			const XMVECTOR p2 = vertex(x, y + 1);
			const XMVECTOR p3 = vertex(x + 1, y + 1);
			return SegmentHitsTriangle(local_start, segment, p0, p1, p3) || SegmentHitsTriangle(local_start, segment, p0, p3, p2);
		});
		if (is_hit) return true;
	}
	return false;
}

// (x, y)가 속한 칸의 삼각형에서 세 꼭짓점 높이를 비율로 섞어 땅 높이를 구한다.
std::optional<float> LandscapeData::GetGroundHeight(float x, float y) const
{
	if (!is_loaded_) return std::nullopt;
	for (const TerrainHeightfield& heightfield : heightfields_) {
		if (x < heightfield.world_min.x || x > heightfield.world_max.x || y < heightfield.world_min.y || y > heightfield.world_max.y) continue;
		XMFLOAT3 local;
		XMStoreFloat3(&local, XMVector3TransformCoord(XMVectorSet(x, y, 0.0f, 1.0f), XMLoadFloat4x4(&heightfield.world_to_local)));
		const float scale = heightfield.collision_scale;
		const int size = static_cast<int>(heightfield.size);
		const float cell_x = local.x / scale;
		const float cell_y = local.y / scale;
		if (cell_x < 0.0f || cell_y < 0.0f || cell_x > size || cell_y > size) continue;
		const int column = std::min(static_cast<int>(cell_x), size - 1);
		const int row = std::min(static_cast<int>(cell_y), size - 1);
		if (heightfield.quad_flags[static_cast<size_t>(row) * size + column] & QUAD_NO_COLLISION) return std::nullopt;
		const auto height = [&](int dx, int dy)
		{
			return (heightfield.heights[static_cast<size_t>(row + dy) * (size + 1) + column + dx] - 32768.0f) / 128.0f;
		};
		const float fx = cell_x - column;
		const float fy = cell_y - row;
		// (0,0)–(1,1) 대각선 아래쪽 삼각형은 (0,0)·(1,0)·(1,1), 위쪽은 (0,0)·(1,1)·(0,1)이다.
		const float local_height = fx >= fy
			? height(0, 0) + fx * (height(1, 0) - height(0, 0)) + fy * (height(1, 1) - height(1, 0))
			: height(0, 0) + fx * (height(1, 1) - height(0, 1)) + fy * (height(0, 1) - height(0, 0));
		XMFLOAT3 world;
		XMStoreFloat3(&world, XMVector3TransformCoord(XMVectorSet(cell_x * scale, cell_y * scale, local_height, 1.0f), XMLoadFloat4x4(&heightfield.local_to_world)));
		return world.z;
	}
	return std::nullopt;
}
