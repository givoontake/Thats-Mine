#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include "HintObjectTree.h"
#include "MapData.h"

// 메시·폴리지 종류 이름과 힌트 종류. 눈 덮인 바위·가문비도 같은 모양이라 같은 종류로 묶는다.
// 색 데이지·버섯은 같은 메시에 재질만 바꾼 폴리지 종류 이름으로 구분하고, 지형 풀로 자라는 데이지(SM_DaisyFlowers)는 흰 데이지다.
struct HintMesh {
	const char* name;
	HintObjectKind kind;
};
static constexpr HintMesh HINT_MESHES[] = {
	{ "SM_RockLarge_01_RVT", HintObjectKind::LARGE_ROCK },
	{ "SM_RockLarge_02_RVT", HintObjectKind::LARGE_ROCK },
	{ "SM_RockLarge_Snow_01", HintObjectKind::LARGE_ROCK },
	{ "SM_RockLarge_Snow_02", HintObjectKind::LARGE_ROCK },
	{ "SM_RockMedium_01_RVT", HintObjectKind::SMALL_ROCK },
	{ "SM_RockMedium_02_RVT", HintObjectKind::SMALL_ROCK },
	{ "SM_RockMedium_Snow_01", HintObjectKind::SMALL_ROCK },
	{ "SM_RockMedium_Snow_02", HintObjectKind::SMALL_ROCK },
	{ "SM_SS_OakTree01", HintObjectKind::OAK },
	{ "SM_SS_OakTree01_RVT", HintObjectKind::OAK },
	{ "SM_SS_OakTree02_RVT", HintObjectKind::OAK },
	{ "SM_SS_OakTreeTall_RVT", HintObjectKind::OAK },
	{ "SM_SS_SpruceTree_RVT", HintObjectKind::SPRUCE },
	{ "SM_SS_SpruceTree_Tall_RVT", HintObjectKind::SPRUCE },
	{ "SM_SS_Spruce_Snow_Tree_RVT", HintObjectKind::SPRUCE },
	{ "SM_SS_Spruce_Snow_Tree_Tall_RVT", HintObjectKind::SPRUCE },
	{ "SM_SS_BirchTree_RVT", HintObjectKind::BIRCH },
	{ "SM_SS_PoplarTree_RVT", HintObjectKind::POPLAR },
	{ "SM_tree_02", HintObjectKind::MAPLE },
	{ "SM_bamboo_01", HintObjectKind::BAMBOO },
	{ "SM_DaisyFlowers", HintObjectKind::DAISY_WHITE },
	{ "SM_DaisyFlowers_Red_FT", HintObjectKind::DAISY_RED },
	{ "SM_DaisyFlowers_Magenta_FT", HintObjectKind::DAISY_MAGENTA },
	{ "SM_DaisyFlowers_Blue_FT", HintObjectKind::DAISY_BLUE },
	{ "SM_DaisyFlowers_Yellow_FT", HintObjectKind::DAISY_YELLOW },
	{ "SM_DaisyFlowers_White_FT", HintObjectKind::DAISY_WHITE },
	{ "SM_Mushrooms_Red_FT", HintObjectKind::MUSHROOM_RED },
	{ "SM_Mushrooms_Purple_FT", HintObjectKind::MUSHROOM_PURPLE },
	{ "SM_Mushrooms_Blue_FT", HintObjectKind::MUSHROOM_BLUE },
};
// 이 개수 이하의 구간은 더 나누지 않고 점을 직접 비교한다.
constexpr std::size_t HINT_TREE_LEAF_SIZE = 8;

static bool IsInsideMap(float x, float y)
{
	return x >= MAP_MIN[0] && x <= MAP_MAX[0] && y >= MAP_MIN[1] && y <= MAP_MAX[1];
}

bool HintObjectTree::Build(const MapData& map)
{
	is_loaded_ = false;
	points_.clear();
	const CollisionObjectData& collision_objects = map.GetCollisionObjects();
	const DecorObjectData& decor_objects = map.GetDecorObjects();
	for (const HintMesh& mesh : HINT_MESHES) {
		// 충돌 있는 물체(바위·나무)는 충돌 모양 원본에서, 충돌 없는 물체(대나무·데이지·버섯)는 충돌 없는 물체 메시에서 찾는다.
		if (const TerrainSource* source = collision_objects.FindSource(mesh.name)) {
			for (std::uint32_t index = source->first_object; index < source->first_object + source->object_count; ++index) {
				const DirectX::XMFLOAT3 location = collision_objects.GetObjectLocation(index);
				if (IsInsideMap(location.x, location.y)) points_.push_back({ location.x, location.y, location.z, mesh.kind });
			}
		}
		else if (const DecorMesh* decor_mesh = decor_objects.FindMesh(mesh.name)) {
			for (std::uint32_t index = decor_mesh->first_location; index < decor_mesh->first_location + decor_mesh->location_count; ++index) {
				const DirectX::XMFLOAT3& location = decor_objects.GetLocations()[index];
				if (IsInsideMap(location.x, location.y)) points_.push_back({ location.x, location.y, location.z, mesh.kind });
			}
		}
		else {
			return false;
		}
	}
	points_.shrink_to_fit();
	BuildRange(0, points_.size(), 0);
	is_loaded_ = true;
	return true;
}

// 구간의 가운데 자리에 축 기준 중앙값 점을 두고, 그보다 작은 점은 앞쪽, 큰 점은 뒤쪽에 모은 뒤 양쪽을 다음 축으로 나눈다.
// 가운데 점은 그 자리에 남겨 기준선으로 쓰고, 자식 구간에는 넣지 않는다.
void HintObjectTree::BuildRange(std::size_t begin, std::size_t end, int depth)
{
	if (end - begin <= HINT_TREE_LEAF_SIZE) return;
	const std::size_t middle = begin + (end - begin) / 2;
	const bool is_x = depth % 2 == 0;
	std::nth_element(points_.begin() + begin, points_.begin() + middle, points_.begin() + end, [is_x](const HintPoint& first, const HintPoint& second)
	{
		return is_x ? first.x < second.x : first.y < second.y;
	});
	BuildRange(begin, middle, depth + 1);
	BuildRange(middle + 1, end, depth + 1);
}

// 기준선인 가운데 점을 세고, 찾는 구가 기준선 양쪽에 걸치는 쪽 구간만 내려간다.
void HintObjectTree::CountRange(std::size_t begin, std::size_t end, int depth, float x, float y, float z, float radius_sq, HintKindCounts& counts) const
{
	const auto count_point = [&](const HintPoint& point)
	{
		const float dx = point.x - x;
		const float dy = point.y - y;
		const float dz = point.z - z;
		if (dx * dx + dy * dy + dz * dz <= radius_sq) ++counts[static_cast<std::size_t>(point.kind)];
	};
	if (end - begin <= HINT_TREE_LEAF_SIZE) {
		for (std::size_t index = begin; index < end; ++index) count_point(points_[index]);
		return;
	}
	const std::size_t middle = begin + (end - begin) / 2;
	const HintPoint& split = points_[middle];
	count_point(split);
	const bool is_x = depth % 2 == 0;
	const float gap = is_x ? x - split.x : y - split.y;
	if (gap < 0.0f || gap * gap <= radius_sq) CountRange(begin, middle, depth + 1, x, y, z, radius_sq, counts);
	if (gap >= 0.0f || gap * gap <= radius_sq) CountRange(middle + 1, end, depth + 1, x, y, z, radius_sq, counts);
}

HintKindCounts HintObjectTree::CountKinds(float x, float y, float z, float radius) const
{
	HintKindCounts counts{};
	if (!is_loaded_ || points_.empty()) return counts;
	CountRange(0, points_.size(), 0, x, y, z, radius * radius, counts);
	for (std::size_t kind = 0; kind < counts.size(); ++kind) {
		const HintObjectKind base = GetBaseHintObjectKind(static_cast<HintObjectKind>(kind));
		if (base != static_cast<HintObjectKind>(kind)) counts[static_cast<std::size_t>(base)] += counts[kind];
	}
	return counts;
}
