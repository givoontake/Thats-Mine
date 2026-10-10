#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <span>
#include <string>
#include <utility>
#include "CollisionObjectData.h"
#include "MapFileReader.h"
#include "hitbox_raycast.h"
#include "map_geometry.h"

using namespace DirectX;

// 탑과 기초의 충돌 모양 원본 이름. 동양 유적이 같은 메시를 써서 맵 데이터 파일에 들어 있다.
constexpr const char* TOWER_SOURCE_NAMES[TOWER_KIND_COUNT] = { "SM_tower_02", "SM_tower_04" }; // TowerKind 순서
constexpr const char* TOWER_FOUNDATION_SOURCE_NAME = "SM_tower_foundation_03";
// 배치물 공간 격자 한 칸의 크기(cm). 가로나 세로가 LARGE_OBJECT_SIZE보다 큰 배치물은 격자에 넣지 않는다.
constexpr float GRID_CELL_SIZE = 1600.0f;
constexpr float LARGE_OBJECT_SIZE = 20000.0f;
// 상자 나무 끝 노드의 최대 삼각형 수
constexpr std::uint32_t BVH_LEAF_SIZE = 4;
constexpr int BVH_STACK_SIZE = 64;
// 캡슐 겹침 검사의 GJK 반복 한도와, 더 가까워지지 않는다고 볼 비율
constexpr int GJK_MAX_ITERATIONS = 32;
constexpr float GJK_EPSILON = 1e-6f;

// 여러 칸에 걸친 배치물을 한 번만 보도록, 스레드마다 배치물별 마지막 검사 번호를 둔다.
static thread_local std::vector<std::uint32_t> visited_objects;
static thread_local std::uint32_t visit_query = 0;

// 선분이 볼록 도형을 지나는지 본다. 면마다 선분을 안쪽 부분만 남기도록 잘라, 남는 구간이 있으면 지난다.
static bool SegmentHitsConvex(const XMFLOAT3& start, const XMFLOAT3& end, const TerrainConvex& convex)
{
	const XMVECTOR from = XMLoadFloat3(&start);
	const XMVECTOR delta = XMVectorSubtract(XMLoadFloat3(&end), from);
	float enter = 0.0f;
	float exit = 1.0f;
	for (const TerrainPlane& plane : convex.planes) {
		const XMVECTOR normal = XMLoadFloat3(&plane.normal);
		const float approach = XMVectorGetX(XMVector3Dot(normal, delta));
		const float outside = XMVectorGetX(XMVector3Dot(normal, from)) - plane.distance;
		if (std::fabs(approach) < PARALLEL_EPSILON) {
			if (outside > 0.0f) return false;
			continue;
		}
		const float t = -outside / approach;
		if (approach < 0.0f) enter = std::max(enter, t);
		else exit = std::min(exit, t);
		if (enter > exit) return false;
	}
	return !convex.planes.empty();
}

// 두 축 정렬 상자가 겹치는지 본다.
static bool BoxesOverlap(const XMFLOAT3& first_min, const XMFLOAT3& first_max, const XMFLOAT3& second_min, const XMFLOAT3& second_max)
{
	return first_min.x <= second_max.x && first_max.x >= second_min.x && first_min.y <= second_max.y && first_max.y >= second_min.y && first_min.z <= second_max.z && first_max.z >= second_min.z;
}

// 점 목록이 이루는 볼록 도형에서 direction 쪽으로 가장 먼 점
static XMVECTOR GetSupport(std::span<const XMVECTOR> points, FXMVECTOR direction)
{
	XMVECTOR best = points[0];
	float best_dot = XMVectorGetX(XMVector3Dot(best, direction));
	for (size_t i = 1; i < points.size(); ++i) {
		const float dot = XMVectorGetX(XMVector3Dot(points[i], direction));
		if (dot > best_dot) {
			best = points[i];
			best_dot = dot;
		}
	}
	return best;
}

// GJK 단체. 두 도형의 차(한 도형의 점 − 다른 도형의 점)에 속한 점 1~4개와, 단체에서 원점에 가장 가까운 점이다.
// 가장 가까운 점을 만드는 데 필요한 점만 남긴다.
struct GjkSimplex {
	std::array<XMVECTOR, 4> points;
	XMVECTOR closest;
	int count;
};

static GjkSimplex ReduceSegment(FXMVECTOR a, FXMVECTOR b)
{
	const XMVECTOR ab = XMVectorSubtract(b, a);
	const float length_sq = XMVectorGetX(XMVector3LengthSq(ab));
	const float t = length_sq > 0.0f ? -XMVectorGetX(XMVector3Dot(a, ab)) / length_sq : 0.0f;
	if (t <= 0.0f) return { { a }, a, 1 };
	if (t >= 1.0f) return { { b }, b, 1 };
	return { { a, b }, XMVectorAdd(a, XMVectorScale(ab, t)), 2 };
}

// 삼각형에서 원점에 가장 가까운 점. 원점이 꼭짓점·변·면 중 어느 영역에 드는지 차례로 가려 낸다.
static GjkSimplex ReduceTriangle(FXMVECTOR a, FXMVECTOR b, FXMVECTOR c)
{
	const XMVECTOR ab = XMVectorSubtract(b, a);
	const XMVECTOR ac = XMVectorSubtract(c, a);
	const float d1 = -XMVectorGetX(XMVector3Dot(ab, a));
	const float d2 = -XMVectorGetX(XMVector3Dot(ac, a));
	if (d1 <= 0.0f && d2 <= 0.0f) return { { a }, a, 1 };
	const float d3 = -XMVectorGetX(XMVector3Dot(ab, b));
	const float d4 = -XMVectorGetX(XMVector3Dot(ac, b));
	if (d3 >= 0.0f && d4 <= d3) return { { b }, b, 1 };
	const float vc = d1 * d4 - d3 * d2;
	if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) return { { a, b }, XMVectorAdd(a, XMVectorScale(ab, d1 / (d1 - d3))), 2 };
	const float d5 = -XMVectorGetX(XMVector3Dot(ab, c));
	const float d6 = -XMVectorGetX(XMVector3Dot(ac, c));
	if (d6 >= 0.0f && d5 <= d6) return { { c }, c, 1 };
	const float vb = d5 * d2 - d1 * d6;
	if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) return { { a, c }, XMVectorAdd(a, XMVectorScale(ac, d2 / (d2 - d6))), 2 };
	const float va = d3 * d6 - d5 * d4;
	if (va <= 0.0f && d4 - d3 >= 0.0f && d5 - d6 >= 0.0f) return { { b, c }, XMVectorAdd(b, XMVectorScale(XMVectorSubtract(c, b), (d4 - d3) / ((d4 - d3) + (d5 - d6)))), 2 };
	// 세 점이 한 줄에 있으면 면이 없으므로 변 중 가장 가까운 것을 쓴다.
	const float sum = va + vb + vc;
	if (sum <= 0.0f) {
		GjkSimplex best = ReduceSegment(a, b);
		for (const GjkSimplex& edge : { ReduceSegment(a, c), ReduceSegment(b, c) }) {
			if (XMVectorGetX(XMVector3LengthSq(edge.closest)) < XMVectorGetX(XMVector3LengthSq(best.closest))) best = edge;
		}
		return best;
	}
	return { { a, b, c }, XMVectorAdd(a, XMVectorAdd(XMVectorScale(ab, vb / sum), XMVectorScale(ac, vc / sum))), 3 };
}

// 사면체에서 원점에 가장 가까운 점. 원점이 바깥쪽에 있는 면만 보고, 바깥쪽 면이 없으면 원점이 안에 있다.
static GjkSimplex ReduceTetrahedron(FXMVECTOR a, FXMVECTOR b, FXMVECTOR c, GXMVECTOR d)
{
	// 면 세 점과, 그 면의 반대편 꼭짓점
	const XMVECTOR faces[4][4] = { { a, b, c, d }, { a, c, d, b }, { a, d, b, c }, { b, d, c, a } };
	GjkSimplex best{ { a, b, c, d }, XMVectorZero(), 4 };
	float best_distance_sq = std::numeric_limits<float>::infinity();
	for (const auto& face : faces) {
		const XMVECTOR normal = XMVector3Cross(XMVectorSubtract(face[1], face[0]), XMVectorSubtract(face[2], face[0]));
		const float origin_side = -XMVectorGetX(XMVector3Dot(face[0], normal));
		const float opposite_side = XMVectorGetX(XMVector3Dot(XMVectorSubtract(face[3], face[0]), normal));
		if (origin_side == 0.0f || origin_side * opposite_side > 0.0f) continue;
		const GjkSimplex reduced = ReduceTriangle(face[0], face[1], face[2]);
		const float distance_sq = XMVectorGetX(XMVector3LengthSq(reduced.closest));
		if (distance_sq < best_distance_sq) {
			best = reduced;
			best_distance_sq = distance_sq;
		}
	}
	return best;
}

// 두 볼록 도형(꼭짓점 목록) 사이 최단 거리가 distance 이하인지 GJK 방식으로 본다.
// 두 도형의 차에서 원점에 가장 가까운 점을 좁혀 가며, 그 점까지 거리가 distance 이하가 되면 겹치고 거리의 하한이 distance를 넘으면 떨어져 있다.
static bool IsConvexWithinDistance(std::span<const XMVECTOR> first, std::span<const XMVECTOR> second, float distance)
{
	const float distance_sq = distance * distance;
	GjkSimplex simplex{ {}, XMVectorSubtract(first[0], second[0]), 0 };
	for (int iteration = 0; iteration < GJK_MAX_ITERATIONS; ++iteration) {
		const XMVECTOR v = simplex.closest;
		const float v_sq = XMVectorGetX(XMVector3LengthSq(v));
		if (v_sq <= distance_sq) return true;
		const XMVECTOR w = XMVectorSubtract(GetSupport(first, XMVectorNegate(v)), GetSupport(second, v));
		const float v_dot_w = XMVectorGetX(XMVector3Dot(v, w));
		// 차의 모든 점은 v 방향으로 v·w 이상이므로 v·w / |v|가 실제 거리의 하한이다.
		if (v_dot_w > 0.0f && v_dot_w * v_dot_w > distance_sq * v_sq) return false;
		// 더 가까워지지 않으면 지금 거리가 최단 거리다.
		if (v_sq - v_dot_w <= GJK_EPSILON * v_sq) return false;
		simplex.points[simplex.count++] = w;
		const std::array<XMVECTOR, 4> p = simplex.points;
		if (simplex.count == 1) simplex = { { w }, w, 1 };
		else if (simplex.count == 2) simplex = ReduceSegment(p[0], p[1]);
		else if (simplex.count == 3) simplex = ReduceTriangle(p[0], p[1], p[2]);
		else simplex = ReduceTetrahedron(p[0], p[1], p[2], p[3]);
		if (simplex.count == 4) return true;
	}
	return XMVectorGetX(XMVector3LengthSq(simplex.closest)) <= distance_sq;
}

// 새 검사 번호를 정한다. 배치물 수가 바뀌었거나 번호가 한 바퀴 돌면 기록을 비운다.
static std::uint32_t BeginObjectVisit(size_t object_count)
{
	if (visited_objects.size() != object_count) {
		visited_objects.assign(object_count, 0);
		visit_query = 0;
	}
	if (++visit_query == 0) {
		std::fill(visited_objects.begin(), visited_objects.end(), 0);
		visit_query = 1;
	}
	return visit_query;
}

// 삼각형 order[begin, end)로 상자 나무 노드를 만들고 번호를 돌려준다.
// 삼각형 중심점이 가장 넓게 퍼진 축을 골라 중심점 순서로 반씩 나누고, BVH_LEAF_SIZE개 이하가 되면 끝 노드로 둔다.
static std::uint32_t BuildBvhNode(TerrainSource& source, std::vector<std::uint32_t>& order, const std::vector<XMFLOAT3>& centers, std::uint32_t begin, std::uint32_t end)
{
	constexpr float INF = std::numeric_limits<float>::infinity();
	const std::uint32_t index = static_cast<std::uint32_t>(source.nodes.size());
	source.nodes.push_back({});
	XMFLOAT3 min{ INF, INF, INF };
	XMFLOAT3 max{ -INF, -INF, -INF };
	XMFLOAT3 center_min{ INF, INF, INF };
	XMFLOAT3 center_max{ -INF, -INF, -INF };
	for (std::uint32_t i = begin; i < end; ++i) {
		for (int corner = 0; corner < 3; ++corner) ExpandBounds(min, max, source.vertices[source.triangles[order[i] * 3 + corner]]);
		ExpandBounds(center_min, center_max, centers[order[i]]);
	}
	int axis = 0;
	for (int candidate = 1; candidate < 3; ++candidate) {
		if (GetAxis(center_max, candidate) - GetAxis(center_min, candidate) > GetAxis(center_max, axis) - GetAxis(center_min, axis)) axis = candidate;
	}
	// 중심점이 모두 한 곳에 모여 있으면 더 나눌 수 없으므로 끝 노드로 둔다.
	if (end - begin <= BVH_LEAF_SIZE || GetAxis(center_max, axis) - GetAxis(center_min, axis) < 1e-4f) {
		source.nodes[index] = { min, max, begin, end - begin };
		return index;
	}
	const std::uint32_t middle = begin + (end - begin) / 2;
	std::nth_element(order.begin() + begin, order.begin() + middle, order.begin() + end, [&](std::uint32_t a, std::uint32_t b) { return GetAxis(centers[a], axis) < GetAxis(centers[b], axis); });
	BuildBvhNode(source, order, centers, begin, middle);
	const std::uint32_t right = BuildBvhNode(source, order, centers, middle, end);
	source.nodes[index] = { min, max, right, 0 };
	return index;
}

// 메시 삼각형으로 상자 나무를 만들고, 삼각형을 끝 노드 순서로 다시 늘어놓는다.
static void BuildBvh(TerrainSource& source)
{
	const std::uint32_t count = static_cast<std::uint32_t>(source.triangles.size() / 3);
	if (count == 0) return;
	std::vector<XMFLOAT3> centers(count);
	for (std::uint32_t triangle = 0; triangle < count; ++triangle) {
		const XMFLOAT3& a = source.vertices[source.triangles[triangle * 3]];
		const XMFLOAT3& b = source.vertices[source.triangles[triangle * 3 + 1]];
		const XMFLOAT3& c = source.vertices[source.triangles[triangle * 3 + 2]];
		centers[triangle] = { (a.x + b.x + c.x) / 3.0f, (a.y + b.y + c.y) / 3.0f, (a.z + b.z + c.z) / 3.0f };
	}
	std::vector<std::uint32_t> order(count);
	std::iota(order.begin(), order.end(), 0u);
	source.nodes.reserve(count);
	BuildBvhNode(source, order, centers, 0, count);
	std::vector<std::uint32_t> sorted(source.triangles.size());
	for (std::uint32_t i = 0; i < count; ++i) {
		for (int corner = 0; corner < 3; ++corner) sorted[i * 3 + corner] = source.triangles[order[i] * 3 + corner];
	}
	source.triangles = std::move(sorted);
}

// 볼록 도형 꼭짓점과 삼각형 목록으로 면 평면을 만든다. 법선은 꼭짓점 평균(도형 안쪽 점)의 반대쪽을 보게 맞춘다.
static TerrainConvex BuildConvex(const std::vector<XMFLOAT3>& vertices, const std::vector<std::uint32_t>& indices)
{
	TerrainConvex convex;
	XMVECTOR inside = XMVectorZero();
	for (const XMFLOAT3& vertex : vertices) inside = XMVectorAdd(inside, XMLoadFloat3(&vertex));
	inside = XMVectorScale(inside, 1.0f / std::max<size_t>(vertices.size(), 1));
	for (size_t i = 0; i + 2 < indices.size(); i += 3) {
		if (indices[i] >= vertices.size() || indices[i + 1] >= vertices.size() || indices[i + 2] >= vertices.size()) continue;
		const XMVECTOR a = XMLoadFloat3(&vertices[indices[i]]);
		XMVECTOR normal = XMVector3Cross(XMVectorSubtract(XMLoadFloat3(&vertices[indices[i + 1]]), a), XMVectorSubtract(XMLoadFloat3(&vertices[indices[i + 2]]), a));
		if (XMVectorGetX(XMVector3LengthSq(normal)) < 1e-12f) continue;
		normal = XMVector3Normalize(normal);
		if (XMVectorGetX(XMVector3Dot(normal, XMVectorSubtract(inside, a))) > 0.0f) normal = XMVectorNegate(normal);
		TerrainPlane plane;
		XMStoreFloat3(&plane.normal, normal);
		plane.distance = XMVectorGetX(XMVector3Dot(normal, a));
		convex.planes.push_back(plane);
	}
	return convex;
}

bool CollisionObjectData::Load(MapFileReader& reader, const std::vector<XMFLOAT3>& tower_candidates)
{
	constexpr float INF = std::numeric_limits<float>::infinity();
	is_loaded_ = false;

	// 충돌 모양 원본
	sources_.assign(reader.ReadCount(), {});
	for (TerrainSource& source : sources_) {
		source.name = reader.ReadName();
		std::uint8_t trace_mode = 0;
		std::uint8_t is_double_sided = 0;
		reader.ReadValue(trace_mode);
		reader.ReadValue(is_double_sided);
		source.is_double_sided = is_double_sided != 0;
		source.local_min = { INF, INF, INF };
		source.local_max = { -INF, -INF, -INF };
		source.spheres.resize(reader.ReadCount());
		for (TerrainSphere& sphere : source.spheres) reader.ReadValue(sphere);
		source.boxes.resize(reader.ReadCount());
		for (TerrainBox& box : source.boxes) {
			reader.ReadValue(box);
			for (int corner = 0; corner < 8; ++corner) {
				const XMVECTOR local = XMVectorSet((corner & 1) ? box.half_size.x : -box.half_size.x, (corner & 2) ? box.half_size.y : -box.half_size.y, (corner & 4) ? box.half_size.z : -box.half_size.z, 0.0f);
				XMFLOAT3 point;
				XMStoreFloat3(&point, XMVectorAdd(XMLoadFloat3(&box.center), XMVector3Rotate(local, XMLoadFloat4(&box.rotation))));
				ExpandBounds(source.local_min, source.local_max, point);
			}
		}
		source.capsules.resize(reader.ReadCount());
		for (TerrainCapsule& capsule : source.capsules) reader.ReadValue(capsule);
		const std::uint32_t convex_count = reader.ReadCount();
		for (std::uint32_t convex = 0; convex < convex_count && reader.IsValid(); ++convex) {
			std::vector<XMFLOAT3> vertices(reader.ReadCount());
			for (XMFLOAT3& vertex : vertices) {
				reader.ReadValue(vertex);
				ExpandBounds(source.local_min, source.local_max, vertex);
			}
			std::vector<std::uint32_t> indices(reader.ReadCount());
			for (std::uint32_t& index : indices) reader.ReadValue(index);
			source.convexes.push_back(BuildConvex(vertices, indices));
			source.convexes.back().vertices = std::move(vertices);
		}
		source.vertices.resize(reader.ReadCount());
		for (XMFLOAT3& vertex : source.vertices) {
			reader.ReadValue(vertex);
			ExpandBounds(source.local_min, source.local_max, vertex);
		}
		source.triangles.resize(reader.ReadCount());
		for (std::uint32_t& index : source.triangles) {
			reader.ReadValue(index);
			if (index >= source.vertices.size()) reader.Invalidate();
		}
		if (!reader.IsValid()) return false;
		source.triangles.resize(source.triangles.size() / 3 * 3);
		source.has_local_shapes = !source.boxes.empty() || !source.convexes.empty() || !source.triangles.empty();
		BuildBvh(source);
	}

	// 배치 하나의 변환·경계 상자·월드 캡슐을 채운다. 구·캡슐은 언리얼의 크기 배율 규칙대로 월드 캡슐로 바꿔 둔다.
	const auto build_object = [&](TerrainObject& object, FXMMATRIX world, const XMFLOAT4& rotation, const XMFLOAT3& scale)
	{
		XMStoreFloat4x4(&object.local_to_world, world);
		XMStoreFloat4x4(&object.world_to_local, XMMatrixInverse(nullptr, world));
		const TerrainSource& source = sources_[object.source];
		object.world_min = { INF, INF, INF };
		object.world_max = { -INF, -INF, -INF };
		if (source.has_local_shapes) {
			const auto [world_min, world_max] = TransformBounds(source.local_min, source.local_max, world);
			object.world_min = world_min;
			object.world_max = world_max;
		}
		const XMVECTOR object_rotation = XMLoadFloat4(&rotation);
		const float scale_x = std::fabs(scale.x);
		const float scale_y = std::fabs(scale.y);
		const float scale_z = std::fabs(scale.z);
		object.first_capsule = static_cast<std::uint32_t>(object_capsules_.size());
		for (const TerrainSphere& sphere : source.spheres) {
			TerrainWorldCapsule capsule;
			XMStoreFloat3(&capsule.start, XMVector3TransformCoord(XMLoadFloat3(&sphere.center), world));
			capsule.end = capsule.start;
			capsule.radius = sphere.radius * std::min({ scale_x, scale_y, scale_z });
			object_capsules_.push_back(capsule);
		}
		for (const TerrainCapsule& element : source.capsules) {
			// 언리얼 캡슐 배율: 반지름은 x·y 배율 중 큰 값, 전체 길이는 z 배율을 따른다.
			const float half_length = std::max((element.length + element.radius * 2.0f) * scale_z * 0.5f, 0.1f);
			const float radius = std::clamp(element.radius * std::max(scale_x, scale_y), 0.1f, half_length);
			const float cylinder_half = std::max(half_length - radius, 0.05f);
			const XMVECTOR center = XMVector3TransformCoord(XMLoadFloat3(&element.center), world);
			const XMVECTOR axis = XMVector3Rotate(XMVector3Rotate(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), XMLoadFloat4(&element.rotation)), object_rotation);
			TerrainWorldCapsule capsule;
			XMStoreFloat3(&capsule.start, XMVectorSubtract(center, XMVectorScale(axis, cylinder_half)));
			XMStoreFloat3(&capsule.end, XMVectorAdd(center, XMVectorScale(axis, cylinder_half)));
			capsule.radius = radius;
			object_capsules_.push_back(capsule);
		}
		object.capsule_count = static_cast<std::uint32_t>(object_capsules_.size()) - object.first_capsule;
		for (std::uint32_t i = object.first_capsule; i < object_capsules_.size(); ++i) {
			const TerrainWorldCapsule& capsule = object_capsules_[i];
			for (const XMFLOAT3& end : { capsule.start, capsule.end }) {
				ExpandBounds(object.world_min, object.world_max, { end.x - capsule.radius, end.y - capsule.radius, end.z - capsule.radius });
				ExpandBounds(object.world_min, object.world_max, { end.x + capsule.radius, end.y + capsule.radius, end.z + capsule.radius });
			}
		}
	};

	// 배치
	objects_.assign(reader.ReadCount(), {});
	object_capsules_.clear();
	for (TerrainObject& object : objects_) {
		std::uint32_t kind = 0;
		reader.ReadValue(object.source);
		reader.ReadValue(kind);
		reader.ReadValue(object.flags);
		const MapTransform transform = reader.ReadTransform();
		if (!reader.IsValid() || object.source >= sources_.size()) return false;
		build_object(object, transform.ToMatrix(), transform.rotation, transform.scale);
	}
	// 배치를 원본 순서로 줄 세우고(같은 원본 안은 파일 순서), 원본마다 자기 배치의 시작 번호와 개수를 둔다.
	std::stable_sort(objects_.begin(), objects_.end(), [](const TerrainObject& first, const TerrainObject& second) { return first.source < second.source; });
	for (std::uint32_t index = 0; index < objects_.size(); ++index) {
		TerrainSource& source = sources_[objects_[index].source];
		if (source.object_count == 0) source.first_object = index;
		++source.object_count;
	}

	// 발판 후보. 모든 자리를 만들어 두고 검사할 때 방이 넘긴 놓인 번호만 본다. 충돌은 발판 크기의 상자 하나이고 방향은 요 각도만 돌린다.
	const std::uint32_t platform_source = static_cast<std::uint32_t>(sources_.size());
	TerrainSource& platform = sources_.emplace_back();
	const XMFLOAT3 platform_half_size{ PLATFORM_SIZE * 0.5f, PLATFORM_SIZE * 0.5f, PLATFORM_THICKNESS * 0.5f };
	platform.boxes.push_back({ { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0f }, platform_half_size });
	platform.local_min = { -platform_half_size.x, -platform_half_size.y, -platform_half_size.z };
	platform.local_max = platform_half_size;
	platform.has_local_shapes = true;
	for (int index = 0; index < PLATFORM_COORDINATE_COUNT; ++index) {
		const PlatformCoordinate& coordinate = PLATFORM_COORDINATES[index];
		const XMMATRIX world = XMMatrixRotationZ(XMConvertToRadians(coordinate.yaw)) * XMMatrixTranslation(coordinate.x, coordinate.y, coordinate.z);
		TerrainObject& object = candidate_objects_.emplace_back();
		XMStoreFloat4x4(&object.local_to_world, world);
		XMStoreFloat4x4(&object.world_to_local, XMMatrixInverse(nullptr, world));
		const auto [world_min, world_max] = TransformBounds(platform.local_min, platform.local_max, world);
		object.world_min = world_min;
		object.world_max = world_max;
		object.source = platform_source;
		object.first_capsule = static_cast<std::uint32_t>(object_capsules_.size());
		object.capsule_count = 0;
		object.flags = TERRAIN_BLOCKS_VISIBILITY | TERRAIN_BLOCKS_PAWN;
	}

	// 탑 후보. 후보 자리마다 종류별 탑과 기초를 만들어 두고 검사할 때 방이 넘긴 놓인 번호만 본다. 기초는 탑 바닥 바로 아래에 가로·세로를 넓혀 붙인다.
	first_tower_object_ = static_cast<std::uint32_t>(candidate_objects_.size());
	if (!tower_candidates.empty()) {
		const TerrainSource* tower_sources[TOWER_KIND_COUNT];
		for (int kind = 0; kind < TOWER_KIND_COUNT; ++kind) {
			tower_sources[kind] = FindSource(TOWER_SOURCE_NAMES[kind]);
			if (!tower_sources[kind]) return false;
		}
		const TerrainSource* foundation_source = FindSource(TOWER_FOUNDATION_SOURCE_NAME);
		if (!foundation_source) return false;
		const XMFLOAT4 no_rotation{ 0.0f, 0.0f, 0.0f, 1.0f };
		const XMFLOAT3 tower_scale{ 1.0f, 1.0f, 1.0f };
		const XMFLOAT3 foundation_scale{ TOWER_FOUNDATION_WIDTH_SCALE, TOWER_FOUNDATION_WIDTH_SCALE, 1.0f };
		for (std::size_t index = 0; index < tower_candidates.size(); ++index) {
			const XMFLOAT3& location = tower_candidates[index];
			for (const TerrainSource* tower_source : tower_sources) {
				TerrainObject& tower = candidate_objects_.emplace_back();
				tower.source = static_cast<std::uint32_t>(tower_source - sources_.data());
				tower.flags = TERRAIN_BLOCKS_VISIBILITY | TERRAIN_BLOCKS_PAWN;
				build_object(tower, XMMatrixTranslation(location.x, location.y, location.z), no_rotation, tower_scale);
			}
			TerrainObject& foundation = candidate_objects_.emplace_back();
			foundation.source = static_cast<std::uint32_t>(foundation_source - sources_.data());
			foundation.flags = TERRAIN_BLOCKS_VISIBILITY | TERRAIN_BLOCKS_PAWN;
			build_object(foundation, XMMatrixScaling(foundation_scale.x, foundation_scale.y, foundation_scale.z) * XMMatrixTranslation(location.x, location.y, location.z - TOWER_FOUNDATION_HEIGHT), no_rotation, foundation_scale);
		}
	}

	BuildGrid();
	is_loaded_ = true;
	return true;
}

const TerrainSource* CollisionObjectData::FindSource(const std::string& name) const
{
	const auto found = std::find_if(sources_.begin(), sources_.end(), [&](const TerrainSource& source) { return source.name == name; });
	return found != sources_.end() ? &*found : nullptr;
}

XMFLOAT3 CollisionObjectData::GetObjectLocation(std::uint32_t index) const
{
	const XMFLOAT4X4& local_to_world = objects_[index].local_to_world;
	return { local_to_world._41, local_to_world._42, local_to_world._43 };
}

// 배치물을 월드 경계 상자가 겹치는 격자 칸마다 넣는다. 칸별 개수를 먼저 세고 한 배열에 이어 담는다.
void CollisionObjectData::BuildGrid()
{
	constexpr float INF = std::numeric_limits<float>::infinity();
	large_objects_.clear();
	float min_x = INF;
	float min_y = INF;
	float max_x = -INF;
	float max_y = -INF;
	std::vector<std::uint32_t> gridded;
	for (std::uint32_t index = 0; index < objects_.size(); ++index) {
		const TerrainObject& object = objects_[index];
		if (object.world_min.x > object.world_max.x) continue;
		if (object.world_max.x - object.world_min.x > LARGE_OBJECT_SIZE || object.world_max.y - object.world_min.y > LARGE_OBJECT_SIZE) {
			large_objects_.push_back(index);
			continue;
		}
		gridded.push_back(index);
		min_x = std::min(min_x, object.world_min.x);
		min_y = std::min(min_y, object.world_min.y);
		max_x = std::max(max_x, object.world_max.x);
		max_y = std::max(max_y, object.world_max.y);
	}
	if (gridded.empty()) {
		grid_columns_ = 0;
		grid_rows_ = 0;
		return;
	}
	grid_min_x_ = min_x;
	grid_min_y_ = min_y;
	grid_columns_ = std::max(1, static_cast<int>(std::ceil((max_x - min_x) / GRID_CELL_SIZE)));
	grid_rows_ = std::max(1, static_cast<int>(std::ceil((max_y - min_y) / GRID_CELL_SIZE)));
	const auto cell_range = [&](const TerrainObject& object)
	{
		const int x0 = std::clamp(static_cast<int>((object.world_min.x - grid_min_x_) / GRID_CELL_SIZE), 0, grid_columns_ - 1);
		const int y0 = std::clamp(static_cast<int>((object.world_min.y - grid_min_y_) / GRID_CELL_SIZE), 0, grid_rows_ - 1);
		const int x1 = std::clamp(static_cast<int>((object.world_max.x - grid_min_x_) / GRID_CELL_SIZE), 0, grid_columns_ - 1);
		const int y1 = std::clamp(static_cast<int>((object.world_max.y - grid_min_y_) / GRID_CELL_SIZE), 0, grid_rows_ - 1);
		return std::make_pair(std::make_pair(x0, y0), std::make_pair(x1, y1));
	};
	cell_starts_.assign(static_cast<size_t>(grid_columns_) * grid_rows_ + 1, 0);
	for (const std::uint32_t index : gridded) {
		const auto [from, to] = cell_range(objects_[index]);
		for (int y = from.second; y <= to.second; ++y) {
			for (int x = from.first; x <= to.first; ++x) ++cell_starts_[y * grid_columns_ + x + 1];
		}
	}
	for (size_t cell = 1; cell < cell_starts_.size(); ++cell) cell_starts_[cell] += cell_starts_[cell - 1];
	cell_objects_.assign(cell_starts_.back(), 0);
	std::vector<std::uint32_t> filled(cell_starts_.begin(), cell_starts_.end() - 1);
	for (const std::uint32_t index : gridded) {
		const auto [from, to] = cell_range(objects_[index]);
		for (int y = from.second; y <= to.second; ++y) {
			for (int x = from.first; x <= to.first; ++x) cell_objects_[filled[y * grid_columns_ + x]++] = index;
		}
	}
}

// 배치 하나의 실제 모양과 선분을 검사한다. 구·캡슐은 월드 좌표로, 나머지는 선분을 메시 기준 좌표로 옮겨 검사한다.
bool CollisionObjectData::IsSegmentBlockedByObject(const TerrainObject& object, const XMFLOAT3& start, const XMFLOAT3& end) const
{
	const XMVECTOR world_start = XMLoadFloat3(&start);
	const XMVECTOR world_end = XMLoadFloat3(&end);
	for (std::uint32_t i = object.first_capsule; i < object.first_capsule + object.capsule_count; ++i) {
		const TerrainWorldCapsule& capsule = object_capsules_[i];
		if (GetSegmentDistanceSquared(world_start, world_end, XMLoadFloat3(&capsule.start), XMLoadFloat3(&capsule.end)) <= capsule.radius * capsule.radius) return true;
	}
	const TerrainSource& source = sources_[object.source];
	if (!source.has_local_shapes) return false;
	const XMMATRIX world_to_local = XMLoadFloat4x4(&object.world_to_local);
	const XMVECTOR local_start = XMVector3TransformCoord(world_start, world_to_local);
	const XMVECTOR local_end = XMVector3TransformCoord(world_end, world_to_local);
	XMFLOAT3 local_from;
	XMFLOAT3 local_to;
	XMStoreFloat3(&local_from, local_start);
	XMStoreFloat3(&local_to, local_end);

	for (const TerrainBox& box : source.boxes) {
		// 상자 기준 좌표로 옮기면 축 정렬 상자 검사가 된다.
		const XMVECTOR center = XMLoadFloat3(&box.center);
		const XMVECTOR rotation = XMLoadFloat4(&box.rotation);
		XMFLOAT3 box_from;
		XMFLOAT3 box_to;
		XMStoreFloat3(&box_from, XMVector3InverseRotate(XMVectorSubtract(local_start, center), rotation));
		XMStoreFloat3(&box_to, XMVector3InverseRotate(XMVectorSubtract(local_end, center), rotation));
		if (SegmentHitsBox(box_from, box_to, { -box.half_size.x, -box.half_size.y, -box.half_size.z }, box.half_size)) return true;
	}
	for (const TerrainConvex& convex : source.convexes) {
		if (SegmentHitsConvex(local_from, local_to, convex)) return true;
	}
	if (source.nodes.empty()) return false;

	// 상자 나무를 위에서부터 선분이 지나는 상자로만 내려가 끝 노드의 삼각형을 검사한다.
	const XMVECTOR segment = XMVectorSubtract(local_end, local_start);
	std::uint32_t stack[BVH_STACK_SIZE];
	int stack_size = 0;
	stack[stack_size++] = 0;
	while (stack_size > 0) {
		const std::uint32_t index = stack[--stack_size];
		const TerrainBvhNode& node = source.nodes[index];
		if (!SegmentHitsBox(local_from, local_to, node.min, node.max)) continue;
		if (node.count > 0) {
			for (std::uint32_t triangle = node.first; triangle < node.first + node.count; ++triangle) {
				const XMVECTOR a = XMLoadFloat3(&source.vertices[source.triangles[triangle * 3]]);
				const XMVECTOR b = XMLoadFloat3(&source.vertices[source.triangles[triangle * 3 + 1]]);
				const XMVECTOR c = XMLoadFloat3(&source.vertices[source.triangles[triangle * 3 + 2]]);
				// 한쪽 면만 막는 삼각형은 법선과 같은 방향으로 들어오는 선분만 검사한다.
				if (!source.is_double_sided && XMVectorGetX(XMVector3Dot(XMVector3Cross(XMVectorSubtract(b, a), XMVectorSubtract(c, a)), segment)) <= 0.0f) continue;
				if (SegmentHitsTriangle(local_start, segment, a, b, c)) return true;
			}
			continue;
		}
		if (stack_size + 2 > BVH_STACK_SIZE) return false;
		stack[stack_size++] = node.first;
		stack[stack_size++] = index + 1;
	}
	return false;
}

// 발판 후보는 자리 번호와 같은 번호이고, 탑 후보 i는 종류별 탑과 기초다. 놓인 종류의 탑과 기초만 넣는다.
std::vector<std::uint32_t> CollisionObjectData::CollectPlacedObjects(const PlatformLayout& platform_layout, const std::vector<TowerPlacement>& tower_placements) const
{
	std::vector<std::uint32_t> placed_objects;
	for (int index = 0; index < PLATFORM_COORDINATE_COUNT; ++index) {
		if (platform_layout[index]) placed_objects.push_back(static_cast<std::uint32_t>(index));
	}
	for (const TowerPlacement& placement : tower_placements) {
		const std::uint32_t first = first_tower_object_ + static_cast<std::uint32_t>(placement.candidate_index) * (TOWER_KIND_COUNT + 1);
		const std::uint32_t foundation = first + TOWER_KIND_COUNT;
		if (foundation >= candidate_objects_.size()) continue;
		placed_objects.push_back(first + static_cast<std::uint32_t>(placement.kind));
		placed_objects.push_back(foundation);
	}
	return placed_objects;
}

// 놓인 발판·탑을 먼저 보고, 격자보다 큰 배치물, 선분이 지나는 격자 칸을 시작점 쪽부터 따라가며 그 칸의 배치물을 본다. 막히는 것을 찾으면 바로 끝낸다.
bool CollisionObjectData::IsSegmentBlockedByObjects(const XMFLOAT3& start, const XMFLOAT3& end, std::uint8_t flag, const std::vector<std::uint32_t>& placed_objects) const
{
	if (!is_loaded_) return false;
	for (const std::uint32_t index : placed_objects) {
		const TerrainObject& object = candidate_objects_[index];
		if ((object.flags & flag) != 0 && SegmentHitsBox(start, end, object.world_min, object.world_max) && IsSegmentBlockedByObject(object, start, end)) return true;
	}
	const std::uint32_t query = BeginObjectVisit(objects_.size());
	const auto check = [&](std::uint32_t index)
	{
		if (visited_objects[index] == query) return false;
		visited_objects[index] = query;
		const TerrainObject& object = objects_[index];
		return (object.flags & flag) != 0 && SegmentHitsBox(start, end, object.world_min, object.world_max) && IsSegmentBlockedByObject(object, start, end);
	};
	for (const std::uint32_t index : large_objects_) {
		if (check(index)) return true;
	}
	if (grid_columns_ == 0) return false;
	const float grid_max_x = grid_min_x_ + grid_columns_ * GRID_CELL_SIZE;
	const float grid_max_y = grid_min_y_ + grid_rows_ * GRID_CELL_SIZE;
	const auto [enter, exit] = ClipSegment2D(start.x, start.y, end.x, end.y, grid_min_x_, grid_min_y_, grid_max_x, grid_max_y);
	if (enter > exit) return false;
	const float dx = end.x - start.x;
	const float dy = end.y - start.y;
	const float x0 = (start.x + dx * enter - grid_min_x_) / GRID_CELL_SIZE;
	const float y0 = (start.y + dy * enter - grid_min_y_) / GRID_CELL_SIZE;
	const float x1 = (start.x + dx * exit - grid_min_x_) / GRID_CELL_SIZE;
	const float y1 = (start.y + dy * exit - grid_min_y_) / GRID_CELL_SIZE;
	return WalkCells(x0, y0, x1, y1, grid_columns_, grid_rows_, [&](int x, int y)
	{
		const int cell = y * grid_columns_ + x;
		for (std::uint32_t i = cell_starts_[cell]; i < cell_starts_[cell + 1]; ++i) {
			if (check(cell_objects_[i])) return true;
		}
		return false;
	});
}

// 배치 하나와 세로 캡슐(축 bottom→top, 반지름 radius)이 겹치는지 본다. 구·캡슐은 두 축 사이 거리로,
// 상자·볼록 도형·삼각형은 꼭짓점을 월드 좌표로 옮겨 GJK로 본다. min, max는 캡슐의 월드 경계 상자로, 삼각형 상자 나무를 내려갈 때 쓴다.
bool CollisionObjectData::IsCapsuleOverlappingObject(const TerrainObject& object, FXMVECTOR bottom, FXMVECTOR top, float radius, const XMFLOAT3& min, const XMFLOAT3& max) const
{
	for (std::uint32_t i = object.first_capsule; i < object.first_capsule + object.capsule_count; ++i) {
		const TerrainWorldCapsule& capsule = object_capsules_[i];
		const float reach = radius + capsule.radius;
		if (GetSegmentDistanceSquared(bottom, top, XMLoadFloat3(&capsule.start), XMLoadFloat3(&capsule.end)) <= reach * reach) return true;
	}
	const TerrainSource& source = sources_[object.source];
	if (!source.has_local_shapes) return false;
	const XMMATRIX local_to_world = XMLoadFloat4x4(&object.local_to_world);
	const XMVECTOR axis[2] = { bottom, top };
	for (const TerrainBox& box : source.boxes) {
		XMVECTOR corners[8];
		for (int corner = 0; corner < 8; ++corner) {
			const XMVECTOR local = XMVectorSet((corner & 1) ? box.half_size.x : -box.half_size.x, (corner & 2) ? box.half_size.y : -box.half_size.y, (corner & 4) ? box.half_size.z : -box.half_size.z, 0.0f);
			corners[corner] = XMVector3TransformCoord(XMVectorAdd(XMLoadFloat3(&box.center), XMVector3Rotate(local, XMLoadFloat4(&box.rotation))), local_to_world);
		}
		if (IsConvexWithinDistance(axis, corners, radius)) return true;
	}
	thread_local std::vector<XMVECTOR> points;
	for (const TerrainConvex& convex : source.convexes) {
		if (convex.vertices.empty()) continue;
		points.clear();
		for (const XMFLOAT3& vertex : convex.vertices) points.push_back(XMVector3TransformCoord(XMLoadFloat3(&vertex), local_to_world));
		if (IsConvexWithinDistance(axis, points, radius)) return true;
	}
	if (source.nodes.empty()) return false;

	// 캡슐 경계 상자를 메시 기준 좌표로 옮겨, 그 상자와 겹치는 상자 나무 노드로만 내려간다.
	const auto [local_min, local_max] = TransformBounds(min, max, XMLoadFloat4x4(&object.world_to_local));
	std::uint32_t stack[BVH_STACK_SIZE];
	int stack_size = 0;
	stack[stack_size++] = 0;
	while (stack_size > 0) {
		const std::uint32_t index = stack[--stack_size];
		const TerrainBvhNode& node = source.nodes[index];
		if (!BoxesOverlap(local_min, local_max, node.min, node.max)) continue;
		if (node.count > 0) {
			for (std::uint32_t triangle = node.first; triangle < node.first + node.count; ++triangle) {
				const XMVECTOR corners[3] = {
					XMVector3TransformCoord(XMLoadFloat3(&source.vertices[source.triangles[triangle * 3]]), local_to_world),
					XMVector3TransformCoord(XMLoadFloat3(&source.vertices[source.triangles[triangle * 3 + 1]]), local_to_world),
					XMVector3TransformCoord(XMLoadFloat3(&source.vertices[source.triangles[triangle * 3 + 2]]), local_to_world),
				};
				if (IsConvexWithinDistance(axis, corners, radius)) return true;
			}
			continue;
		}
		if (stack_size + 2 > BVH_STACK_SIZE) return false;
		stack[stack_size++] = node.first;
		stack[stack_size++] = index + 1;
	}
	return false;
}

// 놓인 발판·탑을 먼저 보고, 격자보다 큰 배치물, 캡슐 경계 상자가 걸치는 격자 칸의 배치물을 본다. 겹치는 것을 찾으면 바로 끝낸다.
bool CollisionObjectData::IsCapsuleOverlappingObjects(const XMFLOAT3& center, float radius, float half_height, std::uint8_t flag, const std::vector<std::uint32_t>& placed_objects) const
{
	if (!is_loaded_) return false;
	const float axis_half = std::max(half_height - radius, 0.0f);
	const XMVECTOR bottom = XMVectorSet(center.x, center.y, center.z - axis_half, 0.0f);
	const XMVECTOR top = XMVectorSet(center.x, center.y, center.z + axis_half, 0.0f);
	const XMFLOAT3 min{ center.x - radius, center.y - radius, center.z - axis_half - radius };
	const XMFLOAT3 max{ center.x + radius, center.y + radius, center.z + axis_half + radius };
	for (const std::uint32_t index : placed_objects) {
		const TerrainObject& object = candidate_objects_[index];
		if ((object.flags & flag) != 0 && BoxesOverlap(min, max, object.world_min, object.world_max) && IsCapsuleOverlappingObject(object, bottom, top, radius, min, max)) return true;
	}
	const std::uint32_t query = BeginObjectVisit(objects_.size());
	const auto check = [&](std::uint32_t index)
	{
		if (visited_objects[index] == query) return false;
		visited_objects[index] = query;
		const TerrainObject& object = objects_[index];
		return (object.flags & flag) != 0 && BoxesOverlap(min, max, object.world_min, object.world_max) && IsCapsuleOverlappingObject(object, bottom, top, radius, min, max);
	};
	for (const std::uint32_t index : large_objects_) {
		if (check(index)) return true;
	}
	if (grid_columns_ == 0) return false;
	const int x0 = static_cast<int>(std::floor((min.x - grid_min_x_) / GRID_CELL_SIZE));
	const int y0 = static_cast<int>(std::floor((min.y - grid_min_y_) / GRID_CELL_SIZE));
	const int x1 = static_cast<int>(std::floor((max.x - grid_min_x_) / GRID_CELL_SIZE));
	const int y1 = static_cast<int>(std::floor((max.y - grid_min_y_) / GRID_CELL_SIZE));
	if (x1 < 0 || y1 < 0 || x0 >= grid_columns_ || y0 >= grid_rows_) return false;
	for (int y = std::max(y0, 0); y <= std::min(y1, grid_rows_ - 1); ++y) {
		for (int x = std::max(x0, 0); x <= std::min(x1, grid_columns_ - 1); ++x) {
			const int cell = y * grid_columns_ + x;
			for (std::uint32_t i = cell_starts_[cell]; i < cell_starts_[cell + 1]; ++i) {
				if (check(cell_objects_[i])) return true;
			}
		}
	}
	return false;
}
