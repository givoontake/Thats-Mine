#include <algorithm>
#include <array>
#include <cmath>
#include "HintObjectGrid.h"
#include "hint_meshes.h"
#include "MapData.h"

static bool IsInsideMap(float x, float y)
{
	return x >= MAP_MIN[0] && x <= MAP_MAX[0] && y >= MAP_MIN[1] && y <= MAP_MAX[1];
}

// 좌표가 든 칸의 줄 번호. 맵 밖 좌표는 가장 가까운 끝 칸으로 붙인다.
static int GetCellLine(float value, float min, int count)
{
	return std::clamp(static_cast<int>(std::floor((value - min) / HINT_OBJECT_GRID_CELL_SIZE)), 0, count - 1);
}

bool HintObjectGrid::Build(const MapData& map)
{
	is_loaded_ = false;
	points_.clear();
	cell_starts_.clear();
	const CollisionObjectData& collision_objects = map.GetCollisionObjects();
	const DecorObjectData& decor_objects = map.GetDecorObjects();
	// 같은 종류에 이름이 여럿일 수 있어(클라이언트 핑용 메시 이름과 맵 데이터의 폴리지 종류 이름), 종류마다 하나라도 찾으면 된다.
	std::array<bool, static_cast<std::size_t>(HintObjectKind::COUNT)> is_kind_listed{};
	std::array<bool, static_cast<std::size_t>(HintObjectKind::COUNT)> is_kind_found{};
	std::vector<HintPoint> collected;
	for (const HintMesh& mesh : HINT_MESHES) {
		is_kind_listed[static_cast<std::size_t>(mesh.kind)] = true;
		// 충돌 있는 물체(바위·나무)는 충돌 모양 원본에서, 충돌 없는 물체(대나무·데이지·버섯)는 충돌 없는 물체 메시에서 찾는다.
		if (const TerrainSource* source = collision_objects.FindSource(mesh.name)) {
			is_kind_found[static_cast<std::size_t>(mesh.kind)] = true;
			for (std::uint32_t index = source->first_object; index < source->first_object + source->object_count; ++index) {
				const DirectX::XMFLOAT3 location = collision_objects.GetObjectLocation(index);
				if (IsInsideMap(location.x, location.y)) collected.push_back({ location.x, location.y, location.z, mesh.kind });
			}
		}
		else if (const DecorMesh* decor_mesh = decor_objects.FindMesh(mesh.name)) {
			is_kind_found[static_cast<std::size_t>(mesh.kind)] = true;
			for (std::uint32_t index = decor_mesh->first_location; index < decor_mesh->first_location + decor_mesh->location_count; ++index) {
				const DirectX::XMFLOAT3& location = decor_objects.GetLocations()[index];
				if (IsInsideMap(location.x, location.y)) collected.push_back({ location.x, location.y, location.z, mesh.kind });
			}
		}
	}
	if (is_kind_listed != is_kind_found) return false;

	// 칸마다 점 수를 세어 시작 위치를 정한 뒤, 점을 칸 순서로 옮겨 담는다.
	columns_ = static_cast<int>(std::ceil((MAP_MAX[0] - MAP_MIN[0]) / HINT_OBJECT_GRID_CELL_SIZE));
	rows_ = static_cast<int>(std::ceil((MAP_MAX[1] - MAP_MIN[1]) / HINT_OBJECT_GRID_CELL_SIZE));
	const auto cell_of = [this](const HintPoint& point)
	{
		return static_cast<std::size_t>(GetCellLine(point.y, MAP_MIN[1], rows_)) * columns_ + GetCellLine(point.x, MAP_MIN[0], columns_);
	};
	cell_starts_.assign(static_cast<std::size_t>(columns_) * rows_ + 1, 0);
	for (const HintPoint& point : collected) ++cell_starts_[cell_of(point) + 1];
	for (std::size_t cell = 1; cell < cell_starts_.size(); ++cell) cell_starts_[cell] += cell_starts_[cell - 1];
	std::vector<std::uint32_t> next(cell_starts_.begin(), cell_starts_.end() - 1);
	points_.resize(collected.size());
	for (const HintPoint& point : collected) points_[next[cell_of(point)]++] = point;
	is_loaded_ = true;
	return true;
}

HintKindCounts HintObjectGrid::CountKinds(float x, float y, float z, float radius) const
{
	HintKindCounts counts{};
	if (!is_loaded_ || points_.empty()) return counts;
	const float radius_sq = radius * radius;
	const int column_begin = GetCellLine(x - radius, MAP_MIN[0], columns_);
	const int column_end = GetCellLine(x + radius, MAP_MIN[0], columns_);
	const int row_begin = GetCellLine(y - radius, MAP_MIN[1], rows_);
	const int row_end = GetCellLine(y + radius, MAP_MIN[1], rows_);
	for (int row = row_begin; row <= row_end; ++row) {
		for (int column = column_begin; column <= column_end; ++column) {
			const std::size_t cell = static_cast<std::size_t>(row) * columns_ + column;
			for (std::uint32_t index = cell_starts_[cell]; index < cell_starts_[cell + 1]; ++index) {
				const HintPoint& point = points_[index];
				const float dx = point.x - x;
				const float dy = point.y - y;
				const float dz = point.z - z;
				if (dx * dx + dy * dy + dz * dz <= radius_sq) ++counts[static_cast<std::size_t>(point.kind)];
			}
		}
	}
	for (std::size_t kind = 0; kind < counts.size(); ++kind) {
		const HintObjectKind base = GetBaseHintObjectKind(static_cast<HintObjectKind>(kind));
		if (base != static_cast<HintObjectKind>(kind)) counts[static_cast<std::size_t>(base)] += counts[kind];
	}
	return counts;
}
