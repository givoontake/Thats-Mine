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

// 배치물 공간 격자 한 칸의 크기(cm). 가로나 세로가 LARGE_OBJECT_SIZE보다 큰 배치물은 격자에 넣지 않는다.
constexpr float GRID_CELL_SIZE = 1600.0f;
constexpr float LARGE_OBJECT_SIZE = 20000.0f;
// 상자 나무 끝 노드의 최대 삼각형 수
constexpr std::uint32_t BVH_LEAF_SIZE = 4;
constexpr int BVH_STACK_SIZE = 64;

// 여러 칸에 걸친 배치물을 한 번만 보도록, 스레드마다 배치물별 마지막 검사 번호를 둔다.
static thread_local std::vector<std::uint32_t> visited_objects;
static thread_local std::uint32_t visit_query = 0;

// 선분 from + delta·t(t는 0~1)가 볼록 도형을 지나는지 본다. 면(xyz는 바깥쪽 법선, w는 원점에서 면까지 거리)마다
// 선분을 안쪽 부분만 남기도록 잘라, 남는 구간이 있으면 지난다.
static bool SegmentHitsConvex(FXMVECTOR from, FXMVECTOR delta, std::span<const XMVECTOR> planes)
{
	float enter = 0.0f;
	float exit = 1.0f;
	for (const XMVECTOR& plane : planes) {
		const float approach = XMVectorGetX(XMVector3Dot(plane, delta));
		const float outside = XMVectorGetX(XMVector3Dot(plane, from)) - XMVectorGetW(plane);
		if (std::fabs(approach) < PARALLEL_EPSILON) {
			if (outside > 0.0f) return false;
			continue;
		}
		const float t = -outside / approach;
		if (approach < 0.0f) enter = std::max(enter, t);
		else exit = std::min(exit, t);
		if (enter > exit) return false;
	}
	return !planes.empty();
}

// 점 세 개를 지나는 면(xyz는 법선, w는 원점에서 면까지 거리). 법선은 inside 반대쪽을 보게 맞춘다.
static XMVECTOR MakeOutwardPlane(FXMVECTOR a, FXMVECTOR b, FXMVECTOR c, GXMVECTOR inside)
{
	XMVECTOR normal = XMVector3Normalize(XMVector3Cross(XMVectorSubtract(b, a), XMVectorSubtract(c, a)));
	if (XMVectorGetX(XMVector3Dot(normal, XMVectorSubtract(inside, a))) > 0.0f) normal = XMVectorNegate(normal);
	return XMVectorSetW(normal, XMVectorGetX(XMVector3Dot(normal, a)));
}

// 원본 상자의 월드 꼭짓점 8개. 꼭짓점 번호의 비트 0·1·2가 x·y·z 쪽(0이면 −, 1이면 +)이다.
static std::array<XMVECTOR, 8> GetWorldBoxCorners(const TerrainBox& box, FXMMATRIX local_to_world)
{
	std::array<XMVECTOR, 8> corners;
	for (int corner = 0; corner < 8; ++corner) {
		const XMVECTOR local = XMVectorSet((corner & 1) ? box.half_size.x : -box.half_size.x, (corner & 2) ? box.half_size.y : -box.half_size.y, (corner & 4) ? box.half_size.z : -box.half_size.z, 0.0f);
		corners[corner] = XMVector3TransformCoord(XMVectorAdd(XMLoadFloat3(&box.center), XMVector3Rotate(local, XMLoadFloat4(&box.rotation))), local_to_world);
	}
	return corners;
}

// 월드 상자 꼭짓점으로 만든 면 6개. 축마다 그 축 비트가 같은 꼭짓점 넷이 한 면이고, 나머지 두 비트가 다른 세 꼭짓점으로 면을 만든다.
static std::array<XMVECTOR, 6> GetWorldBoxPlanes(const std::array<XMVECTOR, 8>& corners)
{
	XMVECTOR center = XMVectorZero();
	for (const XMVECTOR& corner : corners) center = XMVectorAdd(center, XMVectorScale(corner, 0.125f));
	std::array<XMVECTOR, 6> planes;
	for (int axis = 0; axis < 3; ++axis) {
		const int first_bit = 1 << ((axis + 1) % 3);
		const int second_bit = 1 << ((axis + 2) % 3);
		for (int side = 0; side < 2; ++side) {
			const int base = side ? (1 << axis) : 0;
			planes[axis * 2 + side] = MakeOutwardPlane(corners[base], corners[base | first_bit], corners[base | second_bit], center);
		}
	}
	return planes;
}

// 원본 볼록 도형의 면을 월드로 옮겨 planes에 담는다. 검사마다 할당하지 않도록 호출하는 쪽의 스레드별 버퍼를 다시 쓴다.
// 면 법선은 위치 변환 행렬의 역행렬을 뒤집은 행렬(normal_to_world)로 옮긴다.
static void GetWorldConvexPlanes(const TerrainConvex& convex, FXMMATRIX local_to_world, CXMMATRIX normal_to_world, std::vector<XMVECTOR>& planes)
{
	planes.clear();
	for (const TerrainPlane& plane : convex.planes) {
		const XMVECTOR local_normal = XMLoadFloat3(&plane.normal);
		const XMVECTOR normal = XMVector3Normalize(XMVector3TransformNormal(local_normal, normal_to_world));
		const XMVECTOR point = XMVector3TransformCoord(XMVectorScale(local_normal, plane.distance), local_to_world);
		planes.push_back(XMVectorSetW(normal, XMVectorGetX(XMVector3Dot(normal, point))));
	}
}

// 월드 캡슐. 구는 양 끝이 같은 캡슐이다. 원본 구·캡슐을 계산할 때마다 배치 변환으로 옮겨 만든다.
struct WorldCapsule {
	XMVECTOR ends[2];
	float radius;
};

// 원본 구·캡슐을 월드 캡슐로 옮긴다. 배치 배율은 세 축이 같다고 보고, 변환 행렬 x축 길이를 반지름·길이에 곱한다.
// 캡슐의 반 길이·반지름·원기둥 하한은 언리얼 캡슐 배율 적용과 같다. 옮긴 캡슐을 차례로 visit에 넘기고, visit이 true를 돌려주면 멈추고 true를 돌려준다.
template <typename Visit>
static bool VisitWorldCapsules(const TerrainSource& source, FXMMATRIX local_to_world, Visit visit)
{
	const float scale = XMVectorGetX(XMVector3Length(local_to_world.r[0]));
	for (const TerrainSphere& sphere : source.spheres) {
		const XMVECTOR center = XMVector3TransformCoord(XMLoadFloat3(&sphere.center), local_to_world);
		if (visit(WorldCapsule{ { center, center }, sphere.radius * scale })) return true;
	}
	for (const TerrainCapsule& capsule : source.capsules) {
		const float half_length = std::max((capsule.length + capsule.radius * 2.0f) * scale * 0.5f, 0.1f);
		const float radius = std::clamp(capsule.radius * scale, 0.1f, half_length);
		const float cylinder_half = std::max(half_length - radius, 0.05f);
		const XMVECTOR center = XMVector3TransformCoord(XMLoadFloat3(&capsule.center), local_to_world);
		const XMVECTOR local_axis = XMVector3Rotate(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), XMLoadFloat4(&capsule.rotation));
		const XMVECTOR axis = XMVectorScale(XMVector3Normalize(XMVector3TransformNormal(local_axis, local_to_world)), cylinder_half);
		if (visit(WorldCapsule{ { XMVectorSubtract(center, axis), XMVectorAdd(center, axis) }, radius })) return true;
	}
	return false;
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

bool CollisionObjectData::Load(const std::vector<MapMeshFile>& meshes)
{
	constexpr float INF = std::numeric_limits<float>::infinity();
	is_loaded_ = false;

	// 충돌 모양 원본. 충돌 모양 파일이 있는 항목마다 하나이고, 항목 번호로 원본 번호를 찾는다.
	sources_.clear();
	std::vector<std::uint32_t> mesh_sources(meshes.size(), UINT32_MAX);
	for (std::size_t mesh = 0; mesh < meshes.size(); ++mesh) {
		if (meshes[mesh].collision.empty()) continue;
		MapFileReader reader(meshes[mesh].collision);
		if (!reader.ReadHeader("TMCS", MAP_COLLISION_FILE_VERSION)) return false;
		mesh_sources[mesh] = static_cast<std::uint32_t>(sources_.size());
		TerrainSource& source = sources_.emplace_back();
		source.name = reader.ReadName();
		std::uint8_t trace_mode = 0;
		std::uint8_t is_double_sided = 0;
		reader.ReadValue(trace_mode);
		reader.ReadValue(is_double_sided);
		source.is_double_sided = is_double_sided != 0;
		source.local_min = { INF, INF, INF };
		source.local_max = { -INF, -INF, -INF };
		// 구·캡슐은 양 끝점 둘레를 반지름만큼 넓혀 메시 기준 상자에 넣는다.
		const auto expand_round = [&](FXMVECTOR start, FXMVECTOR end, float radius)
		{
			for (const XMVECTOR& point : { start, end }) {
				XMFLOAT3 center;
				XMStoreFloat3(&center, point);
				ExpandBounds(source.local_min, source.local_max, { center.x - radius, center.y - radius, center.z - radius });
				ExpandBounds(source.local_min, source.local_max, { center.x + radius, center.y + radius, center.z + radius });
			}
		};
		source.spheres.resize(reader.ReadCount());
		for (TerrainSphere& sphere : source.spheres) {
			reader.ReadValue(sphere);
			expand_round(XMLoadFloat3(&sphere.center), XMLoadFloat3(&sphere.center), sphere.radius);
		}
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
		for (TerrainCapsule& capsule : source.capsules) {
			reader.ReadValue(capsule);
			const XMVECTOR center = XMLoadFloat3(&capsule.center);
			const XMVECTOR half_axis = XMVectorScale(XMVector3Rotate(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), XMLoadFloat4(&capsule.rotation)), capsule.length * 0.5f);
			expand_round(XMVectorSubtract(center, half_axis), XMVectorAdd(center, half_axis), capsule.radius);
		}
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
		if (!reader.IsValid() || !reader.IsAtEnd() || source.name != meshes[mesh].name) return false;
		source.triangles.resize(source.triangles.size() / 3 * 3);
		source.has_local_shapes = !source.spheres.empty() || !source.boxes.empty() || !source.capsules.empty() || !source.convexes.empty() || !source.triangles.empty();
		BuildBvh(source);
	}

	// 배치 하나의 변환과 월드 경계 상자를 채운다. 충돌 모양은 원본에만 두고, 계산할 때 이 변환으로 월드에 옮긴다.
	const auto build_object = [&](TerrainObject& object, FXMMATRIX world)
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
	};

	// 배치. 배치 파일 앞부분의 충돌 배치를 그 항목의 충돌 모양 원본에 붙인다. 뒷부분의 충돌 없는 물체는 DecorObjectData가 읽는다.
	objects_.clear();
	for (std::size_t mesh = 0; mesh < meshes.size(); ++mesh) {
		if (meshes[mesh].placements.empty()) continue;
		MapFileReader reader(meshes[mesh].placements);
		if (!reader.ReadHeader("TMPL", MAP_PLACEMENT_FILE_VERSION) || reader.ReadName() != meshes[mesh].name) return false;
		const std::uint32_t count = reader.ReadCount();
		if (count > 0 && mesh_sources[mesh] == UINT32_MAX) return false;
		for (std::uint32_t index = 0; index < count; ++index) {
			TerrainObject& object = objects_.emplace_back();
			object.source = mesh_sources[mesh];
			reader.ReadValue(object.flags);
			const MapTransform transform = reader.ReadTransform();
			if (!reader.IsValid()) return false;
			build_object(object, transform.ToMatrix());
		}
	}
	// 배치를 원본 순서로 줄 세우고(같은 원본 안은 파일 순서), 원본마다 자기 배치의 시작 번호와 개수를 둔다.
	std::stable_sort(objects_.begin(), objects_.end(), [](const TerrainObject& first, const TerrainObject& second) { return first.source < second.source; });
	for (std::uint32_t index = 0; index < objects_.size(); ++index) {
		TerrainSource& source = sources_[objects_[index].source];
		if (source.object_count == 0) source.first_object = index;
		++source.object_count;
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

// 배치 하나의 실제 모양과 선분을 검사한다. 원본 모양을 배치 변환으로 월드에 옮겨 검사한다.
// 삼각형은 선분을 메시 기준 좌표로 옮겨 상자 나무에서 후보만 고르고, 고른 삼각형을 월드로 옮겨 검사한다.
bool CollisionObjectData::IsSegmentBlockedByObject(const TerrainObject& object, const XMFLOAT3& start, const XMFLOAT3& end) const
{
	const TerrainSource& source = sources_[object.source];
	const XMMATRIX local_to_world = XMLoadFloat4x4(&object.local_to_world);
	const XMVECTOR world_start = XMLoadFloat3(&start);
	const XMVECTOR world_end = XMLoadFloat3(&end);
	const XMVECTOR world_segment = XMVectorSubtract(world_end, world_start);
	if (VisitWorldCapsules(source, local_to_world, [&](const WorldCapsule& capsule) { return GetSegmentDistanceSquared(world_start, world_end, capsule.ends[0], capsule.ends[1]) <= capsule.radius * capsule.radius; })) return true;
	for (const TerrainBox& box : source.boxes) {
		if (SegmentHitsConvex(world_start, world_segment, GetWorldBoxPlanes(GetWorldBoxCorners(box, local_to_world)))) return true;
	}
	thread_local std::vector<XMVECTOR> planes;
	const XMMATRIX world_to_local = XMLoadFloat4x4(&object.world_to_local);
	const XMMATRIX normal_to_world = XMMatrixTranspose(world_to_local);
	for (const TerrainConvex& convex : source.convexes) {
		GetWorldConvexPlanes(convex, local_to_world, normal_to_world, planes);
		if (SegmentHitsConvex(world_start, world_segment, planes)) return true;
	}
	if (source.nodes.empty()) return false;

	// 상자 나무를 위에서부터 선분이 지나는 상자로만 내려가 끝 노드의 삼각형을 검사한다.
	const XMVECTOR local_start = XMVector3TransformCoord(world_start, world_to_local);
	const XMVECTOR local_end = XMVector3TransformCoord(world_end, world_to_local);
	const XMVECTOR local_segment = XMVectorSubtract(local_end, local_start);
	XMFLOAT3 local_from;
	XMFLOAT3 local_to;
	XMStoreFloat3(&local_from, local_start);
	XMStoreFloat3(&local_to, local_end);
	std::uint32_t stack[BVH_STACK_SIZE];
	int stack_size = 0;
	stack[stack_size++] = 0;
	while (stack_size > 0) {
		const std::uint32_t index = stack[--stack_size];
		const TerrainBvhNode& node = source.nodes[index];
		if (!SegmentHitsBox(local_from, local_to, node.min, node.max)) continue;
		if (node.count > 0) {
			for (std::uint32_t triangle = node.first; triangle < node.first + node.count; ++triangle) {
				const XMVECTOR local[3] = {
					XMLoadFloat3(&source.vertices[source.triangles[triangle * 3]]),
					XMLoadFloat3(&source.vertices[source.triangles[triangle * 3 + 1]]),
					XMLoadFloat3(&source.vertices[source.triangles[triangle * 3 + 2]]),
				};
				// 한쪽 면만 막는 삼각형은 법선과 같은 방향으로 들어오는 선분만 검사한다. 막는 방향은 메시 기준 좌표에서 본다.
				if (!source.is_double_sided && XMVectorGetX(XMVector3Dot(XMVector3Cross(XMVectorSubtract(local[1], local[0]), XMVectorSubtract(local[2], local[0])), local_segment)) <= 0.0f) continue;
				if (SegmentHitsTriangle(world_start, world_segment, XMVector3TransformCoord(local[0], local_to_world), XMVector3TransformCoord(local[1], local_to_world), XMVector3TransformCoord(local[2], local_to_world))) return true;
			}
			continue;
		}
		if (stack_size + 2 > BVH_STACK_SIZE) return false;
		stack[stack_size++] = node.first;
		stack[stack_size++] = index + 1;
	}
	return false;
}

// 격자보다 큰 배치물, 선분이 지나는 격자 칸을 시작점 쪽부터 따라가며 그 칸의 배치물을 본다. 막히는 것을 찾으면 바로 끝낸다.
bool CollisionObjectData::IsSegmentBlockedByObjects(const XMFLOAT3& start, const XMFLOAT3& end, std::uint8_t flag) const
{
	if (!is_loaded_) return false;
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

// 배치 하나와 세로 캡슐(축 bottom→top, 반지름 radius)이 겹치는지 본다. 원본 모양을 배치 변환으로 월드에 옮겨, 구·캡슐은 두 축 사이 거리로,
// 상자·볼록 도형·삼각형은 GJK로 본다. min, max는 캡슐의 월드 경계 상자로, 삼각형 상자 나무를 내려갈 때 쓴다.
bool CollisionObjectData::IsCapsuleOverlappingObject(const TerrainObject& object, FXMVECTOR bottom, FXMVECTOR top, float radius, const XMFLOAT3& min, const XMFLOAT3& max) const
{
	const TerrainSource& source = sources_[object.source];
	const XMMATRIX local_to_world = XMLoadFloat4x4(&object.local_to_world);
	if (VisitWorldCapsules(source, local_to_world, [&](const WorldCapsule& capsule)
	{
		const float reach = radius + capsule.radius;
		return GetSegmentDistanceSquared(bottom, top, capsule.ends[0], capsule.ends[1]) <= reach * reach;
	})) return true;
	const XMVECTOR axis[2] = { bottom, top };
	for (const TerrainBox& box : source.boxes) {
		if (IsConvexWithinDistance(axis, GetWorldBoxCorners(box, local_to_world), radius)) return true;
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

// 격자보다 큰 배치물, 캡슐 경계 상자가 걸치는 격자 칸의 배치물을 본다. 겹치는 것을 찾으면 바로 끝낸다.
bool CollisionObjectData::IsCapsuleOverlappingObjects(const XMFLOAT3& center, float radius, float half_height, std::uint8_t flag) const
{
	if (!is_loaded_) return false;
	const float axis_half = std::max(half_height - radius, 0.0f);
	const XMVECTOR bottom = XMVectorSet(center.x, center.y, center.z - axis_half, 0.0f);
	const XMVECTOR top = XMVectorSet(center.x, center.y, center.z + axis_half, 0.0f);
	const XMFLOAT3 min{ center.x - radius, center.y - radius, center.z - axis_half - radius };
	const XMFLOAT3 max{ center.x + radius, center.y + radius, center.z + axis_half + radius };
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

// 배치 하나에 캡슐 쓸기를 한다. 원본 모양을 배치 변환으로 월드에 옮겨, 구·캡슐은 월드 캡슐로, 상자·볼록 도형·삼각형은 꼭짓점과 면으로 쓴다.
// 한쪽 면 삼각형의 막는 방향은 메시 기준 좌표에서 본다. 배치가 뒤집혀(음수 배율) 있어도 메시의 바깥쪽 면이 그대로 막는다.
MoveHit CollisionObjectData::SweepCapsuleObject(const TerrainObject& object, const CapsuleSweep& sweep) const
{
	const MoveVector delta = sweep.end - sweep.start;
	MoveHit best;
	best.trace_start = sweep.start;
	best.trace_end = sweep.end;
	const TerrainSource& source = sources_[object.source];
	const XMMATRIX local_to_world = XMLoadFloat4x4(&object.local_to_world);
	const XMMATRIX world_to_local = XMLoadFloat4x4(&object.world_to_local);
	VisitWorldCapsules(source, local_to_world, [&](const WorldCapsule& capsule)
	{
		best = GetFirstBlockingHit(best, SweepConvex(sweep, capsule.ends, capsule.radius, {}, false), delta);
		return false;
	});
	for (const TerrainBox& box : source.boxes) {
		const std::array<XMVECTOR, 8> corners = GetWorldBoxCorners(box, local_to_world);
		best = GetFirstBlockingHit(best, SweepConvex(sweep, corners, 0.0f, GetWorldBoxPlanes(corners), false), delta);
	}
	thread_local std::vector<XMVECTOR> points;
	thread_local std::vector<XMVECTOR> planes;
	const XMMATRIX normal_to_world = XMMatrixTranspose(world_to_local);
	for (const TerrainConvex& convex : source.convexes) {
		if (convex.vertices.empty()) continue;
		points.clear();
		for (const XMFLOAT3& vertex : convex.vertices) points.push_back(XMVector3TransformCoord(XMLoadFloat3(&vertex), local_to_world));
		GetWorldConvexPlanes(convex, local_to_world, normal_to_world, planes);
		best = GetFirstBlockingHit(best, SweepConvex(sweep, points, 0.0f, planes, false), delta);
	}
	if (source.nodes.empty()) return best;

	// 쓸기 범위를 메시 기준 좌표로 옮겨, 그 상자와 겹치는 상자 나무 노드로만 내려간다.
	const auto [local_min, local_max] = TransformBounds(sweep.min, sweep.max, world_to_local);
	const XMVECTOR local_delta = XMVector3TransformNormal(sweep.delta, world_to_local);
	// 뒤집힌 배치는 메시 기준 바깥쪽 면이 월드에서 꼭짓점 순서와 반대쪽을 본다.
	const bool is_mirrored = XMVectorGetX(XMMatrixDeterminant(local_to_world)) < 0.0f;
	const XMVECTOR start_center = XMVectorSet(sweep.start.x, sweep.start.y, sweep.start.z, 0.0f);
	std::uint32_t stack[BVH_STACK_SIZE];
	int stack_size = 0;
	stack[stack_size++] = 0;
	while (stack_size > 0) {
		const std::uint32_t index = stack[--stack_size];
		const TerrainBvhNode& node = source.nodes[index];
		if (!BoxesOverlap(local_min, local_max, node.min, node.max)) continue;
		if (node.count > 0) {
			for (std::uint32_t triangle = node.first; triangle < node.first + node.count; ++triangle) {
				const XMVECTOR local[3] = {
					XMLoadFloat3(&source.vertices[source.triangles[triangle * 3]]),
					XMLoadFloat3(&source.vertices[source.triangles[triangle * 3 + 1]]),
					XMLoadFloat3(&source.vertices[source.triangles[triangle * 3 + 2]]),
				};
				const XMVECTOR local_cross = XMVector3Cross(XMVectorSubtract(local[1], local[0]), XMVectorSubtract(local[2], local[0]));
				// 한쪽 면만 막는 삼각형은 꼭짓점 순서의 법선과 같은 방향으로 들어오는 이동만 막는다. 선분 검사와 같은 규칙이다.
				if (!source.is_double_sided && XMVectorGetX(XMVector3Dot(local_cross, local_delta)) <= 0.0f) continue;
				const XMVECTOR corners[3] = { XMVector3TransformCoord(local[0], local_to_world), XMVector3TransformCoord(local[1], local_to_world), XMVector3TransformCoord(local[2], local_to_world) };
				XMVECTOR normal = XMVector3Cross(XMVectorSubtract(corners[1], corners[0]), XMVectorSubtract(corners[2], corners[0]));
				if (XMVectorGetX(XMVector3LengthSq(normal)) < 1e-12f) continue;
				normal = XMVector3Normalize(normal);
				// 바깥쪽 면은 꼭짓점 순서 법선의 반대쪽이다. 양면이면 캡슐이 있는 쪽 면으로 본다.
				if (source.is_double_sided) {
					if (XMVectorGetX(XMVector3Dot(normal, XMVectorSubtract(start_center, corners[0]))) < 0.0f) normal = XMVectorNegate(normal);
				}
				else if (!is_mirrored) {
					normal = XMVectorNegate(normal);
				}
				const XMVECTOR face = XMVectorSetW(normal, XMVectorGetX(XMVector3Dot(normal, corners[0])));
				best = GetFirstBlockingHit(best, SweepConvex(sweep, corners, 0.0f, { &face, 1 }, true), delta);
			}
			continue;
		}
		if (stack_size + 2 > BVH_STACK_SIZE) return best;
		stack[stack_size++] = node.first;
		stack[stack_size++] = index + 1;
	}
	return best;
}

// 격자보다 큰 배치물, 쓸기 범위가 걸치는 격자 칸의 배치물을 모두 쓸어 가장 먼저 막는 결과를 고른다.
MoveHit CollisionObjectData::SweepCapsule(const MoveVector& start, const MoveVector& end, float radius, float half_height, std::uint8_t flag, bool is_ignoring_moving_out) const
{
	MoveHit best;
	best.trace_start = start;
	best.trace_end = end;
	if (!is_loaded_) return best;
	const CapsuleSweep sweep = MakeCapsuleSweep(start, end, radius, half_height, is_ignoring_moving_out);
	const MoveVector delta = end - start;
	const std::uint32_t query = BeginObjectVisit(objects_.size());
	const auto check = [&](std::uint32_t index)
	{
		if (visited_objects[index] == query) return;
		visited_objects[index] = query;
		const TerrainObject& object = objects_[index];
		if ((object.flags & flag) == 0 || !BoxesOverlap(sweep.min, sweep.max, object.world_min, object.world_max)) return;
		best = GetFirstBlockingHit(best, SweepCapsuleObject(object, sweep), delta);
	};
	for (const std::uint32_t index : large_objects_) check(index);
	if (grid_columns_ == 0) return best;
	const int x0 = static_cast<int>(std::floor((sweep.min.x - grid_min_x_) / GRID_CELL_SIZE));
	const int y0 = static_cast<int>(std::floor((sweep.min.y - grid_min_y_) / GRID_CELL_SIZE));
	const int x1 = static_cast<int>(std::floor((sweep.max.x - grid_min_x_) / GRID_CELL_SIZE));
	const int y1 = static_cast<int>(std::floor((sweep.max.y - grid_min_y_) / GRID_CELL_SIZE));
	if (x1 < 0 || y1 < 0 || x0 >= grid_columns_ || y0 >= grid_rows_) return best;
	for (int y = std::max(y0, 0); y <= std::min(y1, grid_rows_ - 1); ++y) {
		for (int x = std::max(x0, 0); x <= std::min(x1, grid_columns_ - 1); ++x) {
			const int cell = y * grid_columns_ + x;
			for (std::uint32_t i = cell_starts_[cell]; i < cell_starts_[cell + 1]; ++i) check(cell_objects_[i]);
		}
	}
	return best;
}
