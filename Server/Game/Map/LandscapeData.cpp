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
	BuildGrid();
	is_loaded_ = true;
	return true;
}

void LandscapeData::BuildGrid()
{
	grid_cells_.clear();
	grid_columns_ = 0;
	grid_rows_ = 0;
	if (heightfields_.empty()) return;
	grid_min_ = { heightfields_.front().world_min.x, heightfields_.front().world_min.y };
	grid_max_ = { heightfields_.front().world_max.x, heightfields_.front().world_max.y };
	grid_cell_size_ = std::min(grid_max_.x - grid_min_.x, grid_max_.y - grid_min_.y);
	for (const TerrainHeightfield& heightfield : heightfields_) {
		grid_min_ = { std::min(grid_min_.x, heightfield.world_min.x), std::min(grid_min_.y, heightfield.world_min.y) };
		grid_max_ = { std::max(grid_max_.x, heightfield.world_max.x), std::max(grid_max_.y, heightfield.world_max.y) };
		grid_cell_size_ = std::min({ grid_cell_size_, heightfield.world_max.x - heightfield.world_min.x, heightfield.world_max.y - heightfield.world_min.y });
	}
	grid_columns_ = static_cast<int>((grid_max_.x - grid_min_.x) / grid_cell_size_) + 1;
	grid_rows_ = static_cast<int>((grid_max_.y - grid_min_.y) / grid_cell_size_) + 1;
	grid_cells_.assign(static_cast<size_t>(grid_columns_) * grid_rows_, {});
	// 조각 범위 양 끝 좌표가 드는 칸 사이를 모두 채운다. 땅 높이를 찾을 때 같은 식으로 칸을 구하므로, 범위 안의 점은 그 조각이 들어 있는 칸에 떨어진다.
	for (std::uint32_t index = 0; index < heightfields_.size(); ++index) {
		const TerrainHeightfield& heightfield = heightfields_[index];
		const int first_column = std::min(static_cast<int>((heightfield.world_min.x - grid_min_.x) / grid_cell_size_), grid_columns_ - 1);
		const int last_column = std::min(static_cast<int>((heightfield.world_max.x - grid_min_.x) / grid_cell_size_), grid_columns_ - 1);
		const int first_row = std::min(static_cast<int>((heightfield.world_min.y - grid_min_.y) / grid_cell_size_), grid_rows_ - 1);
		const int last_row = std::min(static_cast<int>((heightfield.world_max.y - grid_min_.y) / grid_cell_size_), grid_rows_ - 1);
		for (int row = first_row; row <= last_row; ++row) {
			for (int column = first_column; column <= last_column; ++column) grid_cells_[static_cast<size_t>(row) * grid_columns_ + column].push_back(index);
		}
	}
}

// 조각마다 월드 상자를 조각 기준 좌표로 옮겨 걸치는 칸을 고르고, 칸 꼭짓점 높이가 상자 높이 범위에 걸치는 칸만 삼각형을 월드 좌표로 넘긴다.
template <typename Visit>
bool LandscapeData::VisitTriangles(const XMFLOAT3& min, const XMFLOAT3& max, Visit visit) const
{
	for (const TerrainHeightfield& heightfield : heightfields_) {
		if (!BoxesOverlap(min, max, heightfield.world_min, heightfield.world_max)) continue;
		const XMMATRIX local_to_world = XMLoadFloat4x4(&heightfield.local_to_world);
		const auto [local_min, local_max] = TransformBounds(min, max, XMLoadFloat4x4(&heightfield.world_to_local));
		const float scale = heightfield.collision_scale;
		const int size = static_cast<int>(heightfield.size);
		const int x0 = std::max(0, static_cast<int>(std::floor(local_min.x / scale)));
		const int y0 = std::max(0, static_cast<int>(std::floor(local_min.y / scale)));
		const int x1 = std::min(size - 1, static_cast<int>(std::floor(local_max.x / scale)));
		const int y1 = std::min(size - 1, static_cast<int>(std::floor(local_max.y / scale)));
		const auto height = [&](int x, int y)
		{
			return (heightfield.heights[static_cast<size_t>(y) * (size + 1) + x] - 32768.0f) / 128.0f;
		};
		for (int y = y0; y <= y1; ++y) {
			for (int x = x0; x <= x1; ++x) {
				if (heightfield.quad_flags[static_cast<size_t>(y) * size + x] & QUAD_NO_COLLISION) continue;
				const float h0 = height(x, y);
				const float h1 = height(x + 1, y);
				const float h2 = height(x, y + 1);
				const float h3 = height(x + 1, y + 1);
				if (std::max({ h0, h1, h2, h3 }) < local_min.z || std::min({ h0, h1, h2, h3 }) > local_max.z) continue;
				const XMVECTOR p0 = XMVector3TransformCoord(XMVectorSet(x * scale, y * scale, h0, 1.0f), local_to_world);
				const XMVECTOR p1 = XMVector3TransformCoord(XMVectorSet((x + 1) * scale, y * scale, h1, 1.0f), local_to_world);
				const XMVECTOR p2 = XMVector3TransformCoord(XMVectorSet(x * scale, (y + 1) * scale, h2, 1.0f), local_to_world);
				const XMVECTOR p3 = XMVector3TransformCoord(XMVectorSet((x + 1) * scale, (y + 1) * scale, h3, 1.0f), local_to_world);
				if (visit(p0, p1, p3) || visit(p0, p3, p2)) return true;
			}
		}
	}
	return false;
}

MoveHit LandscapeData::SweepCapsule(const MoveVector& start, const MoveVector& end, float radius, float half_height, bool is_ignoring_moving_out) const
{
	MoveHit best;
	best.trace_start = start;
	best.trace_end = end;
	if (!is_loaded_) return best;
	const CapsuleSweep sweep = MakeCapsuleSweep(start, end, radius, half_height, is_ignoring_moving_out);
	const MoveVector delta = end - start;
	VisitTriangles(sweep.min, sweep.max, [&](FXMVECTOR a, FXMVECTOR b, FXMVECTOR c)
	{
		XMVECTOR normal = XMVector3Normalize(XMVector3Cross(XMVectorSubtract(b, a), XMVectorSubtract(c, a)));
		if (XMVectorGetZ(normal) < 0.0f) normal = XMVectorNegate(normal);
		const XMVECTOR face = XMVectorSetW(normal, XMVectorGetX(XMVector3Dot(normal, a)));
		const XMVECTOR corners[3] = { a, b, c };
		best = GetFirstBlockingHit(best, SweepConvex(sweep, corners, 0.0f, { &face, 1 }, true), delta);
		return false;
	});
	return best;
}

// 큰 월드 좌표에서 오차가 생기지 않도록 캡슐 중심을 원점으로 옮겨 삼각형까지 거리를 잰다.
bool LandscapeData::IsCapsuleOverlapping(const MoveVector& center, float radius, float half_height) const
{
	if (!is_loaded_) return false;
	const std::optional<float> ground = GetGroundHeight(center.x, center.y);
	if (ground && center.z - half_height < *ground) return true;
	const float axis_half = std::max(half_height - radius, 0.0f);
	const XMVECTOR origin = XMVectorSet(center.x, center.y, center.z, 0.0f);
	const XMVECTOR axis[2] = { XMVectorSet(0.0f, 0.0f, -axis_half, 0.0f), XMVectorSet(0.0f, 0.0f, axis_half, 0.0f) };
	const XMFLOAT3 min{ center.x - radius, center.y - radius, center.z - half_height };
	const XMFLOAT3 max{ center.x + radius, center.y + radius, center.z + half_height };
	return VisitTriangles(min, max, [&](FXMVECTOR a, FXMVECTOR b, FXMVECTOR c)
	{
		const XMVECTOR corners[3] = { XMVectorSubtract(a, origin), XMVectorSubtract(b, origin), XMVectorSubtract(c, origin) };
		return GetConvexDistance(axis, corners).distance <= radius;
	});
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
	if (!is_loaded_ || x < grid_min_.x || x > grid_max_.x || y < grid_min_.y || y > grid_max_.y) return std::nullopt;
	const int grid_column = std::min(static_cast<int>((x - grid_min_.x) / grid_cell_size_), grid_columns_ - 1);
	const int grid_row = std::min(static_cast<int>((y - grid_min_.y) / grid_cell_size_), grid_rows_ - 1);
	for (const std::uint32_t index : grid_cells_[static_cast<size_t>(grid_row) * grid_columns_ + grid_column]) {
		const TerrainHeightfield& heightfield = heightfields_[index];
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
