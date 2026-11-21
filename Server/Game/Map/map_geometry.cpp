#include <array>
#include <vector>
#include "map_geometry.h"
#include "movement_settings.h"

using namespace DirectX;

float GetAxis(const XMFLOAT3& value, int axis)
{
	return axis == 0 ? value.x : (axis == 1 ? value.y : value.z);
}

void ExpandBounds(XMFLOAT3& min, XMFLOAT3& max, const XMFLOAT3& point)
{
	min = { std::min(min.x, point.x), std::min(min.y, point.y), std::min(min.z, point.z) };
	max = { std::max(max.x, point.x), std::max(max.y, point.y), std::max(max.z, point.z) };
}

// 메시 기준 상자의 꼭짓점 8개를 행렬로 옮겨 감싸는 축 정렬 상자를 만든다.
std::pair<XMFLOAT3, XMFLOAT3> TransformBounds(const XMFLOAT3& min, const XMFLOAT3& max, FXMMATRIX matrix)
{
	constexpr float INF = std::numeric_limits<float>::infinity();
	XMFLOAT3 out_min{ INF, INF, INF };
	XMFLOAT3 out_max{ -INF, -INF, -INF };
	for (int corner = 0; corner < 8; ++corner) {
		const XMVECTOR point = XMVectorSet((corner & 1) ? max.x : min.x, (corner & 2) ? max.y : min.y, (corner & 4) ? max.z : min.z, 1.0f);
		XMFLOAT3 moved;
		XMStoreFloat3(&moved, XMVector3TransformCoord(point, matrix));
		ExpandBounds(out_min, out_max, moved);
	}
	return { out_min, out_max };
}

// 선분 start→end(비율 0~1)가 축 정렬 상자를 지나는지 본다. 축마다 상자 두 면 사이에 드는 비율 구간을 구해 겹치는지 확인한다.
bool SegmentHitsBox(const XMFLOAT3& start, const XMFLOAT3& end, const XMFLOAT3& min, const XMFLOAT3& max)
{
	float enter = 0.0f;
	float exit = 1.0f;
	for (int axis = 0; axis < 3; ++axis) {
		const float from = GetAxis(start, axis);
		const float delta = GetAxis(end, axis) - from;
		const float low = GetAxis(min, axis);
		const float high = GetAxis(max, axis);
		if (std::fabs(delta) < PARALLEL_EPSILON) {
			if (from < low || from > high) return false;
			continue;
		}
		float near_t = (low - from) / delta;
		float far_t = (high - from) / delta;
		if (near_t > far_t) std::swap(near_t, far_t);
		enter = std::max(enter, near_t);
		exit = std::min(exit, far_t);
		if (enter > exit) return false;
	}
	return true;
}

// 선분 start + segment·t(t는 0~1)가 삼각형 a, b, c를 지나는지 본다. 앞뒷면을 가리지 않는다.
// 평면을 뚫는 t와 삼각형 안 비율(u, v)을 한 번에 구하는 묄러-트럼보어 방식이다.
bool SegmentHitsTriangle(FXMVECTOR start, FXMVECTOR segment, FXMVECTOR a, GXMVECTOR b, HXMVECTOR c)
{
	const XMVECTOR edge1 = XMVectorSubtract(b, a);
	const XMVECTOR edge2 = XMVectorSubtract(c, a);
	const XMVECTOR p = XMVector3Cross(segment, edge2);
	const float determinant = XMVectorGetX(XMVector3Dot(edge1, p));
	if (std::fabs(determinant) < PARALLEL_EPSILON) return false;
	const float inverse = 1.0f / determinant;
	const XMVECTOR to_start = XMVectorSubtract(start, a);
	const float u = XMVectorGetX(XMVector3Dot(to_start, p)) * inverse;
	if (u < 0.0f || u > 1.0f) return false;
	const XMVECTOR q = XMVector3Cross(to_start, edge1);
	const float v = XMVectorGetX(XMVector3Dot(segment, q)) * inverse;
	if (v < 0.0f || u + v > 1.0f) return false;
	const float t = XMVectorGetX(XMVector3Dot(edge2, q)) * inverse;
	return t >= 0.0f && t <= 1.0f;
}

// 선분 (x0, y0)→(x1, y1)의 xy가 [min_x, max_x]×[min_y, max_y] 안에 드는 비율 구간. 없으면 앞 값이 뒤 값보다 크다.
std::pair<float, float> ClipSegment2D(float x0, float y0, float x1, float y1, float min_x, float min_y, float max_x, float max_y)
{
	float enter = 0.0f;
	float exit = 1.0f;
	const float starts[2] = { x0, y0 };
	const float deltas[2] = { x1 - x0, y1 - y0 };
	const float lows[2] = { min_x, min_y };
	const float highs[2] = { max_x, max_y };
	for (int axis = 0; axis < 2; ++axis) {
		if (std::fabs(deltas[axis]) < PARALLEL_EPSILON) {
			if (starts[axis] < lows[axis] || starts[axis] > highs[axis]) return { 1.0f, 0.0f };
			continue;
		}
		float near_t = (lows[axis] - starts[axis]) / deltas[axis];
		float far_t = (highs[axis] - starts[axis]) / deltas[axis];
		if (near_t > far_t) std::swap(near_t, far_t);
		enter = std::max(enter, near_t);
		exit = std::min(exit, far_t);
	}
	return { enter, exit };
}

// 두 축 정렬 상자가 겹치는지 본다.
bool BoxesOverlap(const XMFLOAT3& first_min, const XMFLOAT3& first_max, const XMFLOAT3& second_min, const XMFLOAT3& second_max)
{
	return first_min.x <= second_max.x && first_max.x >= second_min.x && first_min.y <= second_max.y && first_max.y >= second_min.y && first_min.z <= second_max.z && first_max.z >= second_min.z;
}

// GJK 단체의 점 하나. 두 도형의 차 점 w = a − b와, 그 점을 만든 두 도형의 점 a, b다.
struct GjkVertex {
	XMVECTOR w;
	XMVECTOR a;
	XMVECTOR b;
};

// 단체에서 원점에 가장 가까운 점을 만드는 데 필요한 점과 각 점의 비율. 가장 가까운 점은 점들을 비율대로 섞은 것이다.
struct GjkClosest {
	std::array<GjkVertex, 4> vertices;
	std::array<float, 4> weights;
	int count;
};

static GjkClosest GetClosestOnSegment(const GjkVertex& a, const GjkVertex& b)
{
	const XMVECTOR ab = XMVectorSubtract(b.w, a.w);
	const float length_sq = XMVectorGetX(XMVector3LengthSq(ab));
	const float t = length_sq > 0.0f ? -XMVectorGetX(XMVector3Dot(a.w, ab)) / length_sq : 0.0f;
	if (t <= 0.0f) return { { a }, { 1.0f }, 1 };
	if (t >= 1.0f) return { { b }, { 1.0f }, 1 };
	return { { a, b }, { 1.0f - t, t }, 2 };
}

static XMVECTOR GetClosestPoint(const GjkClosest& closest)
{
	XMVECTOR point = XMVectorZero();
	for (int i = 0; i < closest.count; ++i) point = XMVectorAdd(point, XMVectorScale(closest.vertices[i].w, closest.weights[i]));
	return point;
}

// 삼각형에서 원점에 가장 가까운 점. 원점이 꼭짓점·변·면 중 어느 영역에 드는지 차례로 가려 낸다.
static GjkClosest GetClosestOnTriangle(const GjkVertex& a, const GjkVertex& b, const GjkVertex& c)
{
	const XMVECTOR ab = XMVectorSubtract(b.w, a.w);
	const XMVECTOR ac = XMVectorSubtract(c.w, a.w);
	const float d1 = -XMVectorGetX(XMVector3Dot(ab, a.w));
	const float d2 = -XMVectorGetX(XMVector3Dot(ac, a.w));
	if (d1 <= 0.0f && d2 <= 0.0f) return { { a }, { 1.0f }, 1 };
	const float d3 = -XMVectorGetX(XMVector3Dot(ab, b.w));
	const float d4 = -XMVectorGetX(XMVector3Dot(ac, b.w));
	if (d3 >= 0.0f && d4 <= d3) return { { b }, { 1.0f }, 1 };
	const float vc = d1 * d4 - d3 * d2;
	if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
		const float t = d1 / (d1 - d3);
		return { { a, b }, { 1.0f - t, t }, 2 };
	}
	const float d5 = -XMVectorGetX(XMVector3Dot(ab, c.w));
	const float d6 = -XMVectorGetX(XMVector3Dot(ac, c.w));
	if (d6 >= 0.0f && d5 <= d6) return { { c }, { 1.0f }, 1 };
	const float vb = d5 * d2 - d1 * d6;
	if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
		const float t = d2 / (d2 - d6);
		return { { a, c }, { 1.0f - t, t }, 2 };
	}
	const float va = d3 * d6 - d5 * d4;
	if (va <= 0.0f && d4 - d3 >= 0.0f && d5 - d6 >= 0.0f) {
		const float t = (d4 - d3) / ((d4 - d3) + (d5 - d6));
		return { { b, c }, { 1.0f - t, t }, 2 };
	}
	// 세 점이 한 줄에 있으면 면이 없으므로 변 중 가장 가까운 것을 쓴다.
	const float sum = va + vb + vc;
	if (sum <= 0.0f) {
		GjkClosest best = GetClosestOnSegment(a, b);
		float best_sq = XMVectorGetX(XMVector3LengthSq(GetClosestPoint(best)));
		for (const GjkClosest& edge : { GetClosestOnSegment(a, c), GetClosestOnSegment(b, c) }) {
			const float distance_sq = XMVectorGetX(XMVector3LengthSq(GetClosestPoint(edge)));
			if (distance_sq < best_sq) {
				best = edge;
				best_sq = distance_sq;
			}
		}
		return best;
	}
	return { { a, b, c }, { va / sum, vb / sum, vc / sum }, 3 };
}

// 사면체에서 원점에 가장 가까운 점. 원점이 바깥쪽에 있는 면만 보고, 바깥쪽 면이 없으면 원점이 안에 있어 점 4개를 그대로 둔다.
// 네 점이 거의 한 평면에 있으면 안팎을 가릴 수 없으므로 네 면 모두에서 가장 가까운 점을 찾는다.
static GjkClosest GetClosestOnTetrahedron(const GjkVertex& a, const GjkVertex& b, const GjkVertex& c, const GjkVertex& d)
{
	// 면 세 점과, 그 면의 반대편 꼭짓점
	const GjkVertex* faces[4][4] = { { &a, &b, &c, &d }, { &a, &c, &d, &b }, { &a, &d, &b, &c }, { &b, &d, &c, &a } };
	const XMVECTOR ab = XMVectorSubtract(b.w, a.w);
	const XMVECTOR ac = XMVectorSubtract(c.w, a.w);
	const XMVECTOR ad = XMVectorSubtract(d.w, a.w);
	const float volume = XMVectorGetX(XMVector3Dot(XMVector3Cross(ab, ac), ad));
	const float extent = XMVectorGetX(XMVector3Length(ab)) * XMVectorGetX(XMVector3Length(ac)) * XMVectorGetX(XMVector3Length(ad));
	const bool is_flat = std::fabs(volume) <= GJK_FLAT_RATIO * extent;
	GjkClosest best{ { a, b, c, d }, { 0.0f, 0.0f, 0.0f, 0.0f }, 4 };
	float best_sq = std::numeric_limits<float>::infinity();
	for (const auto& face : faces) {
		const XMVECTOR normal = XMVector3Cross(XMVectorSubtract(face[1]->w, face[0]->w), XMVectorSubtract(face[2]->w, face[0]->w));
		const float origin_side = -XMVectorGetX(XMVector3Dot(face[0]->w, normal));
		const float opposite_side = XMVectorGetX(XMVector3Dot(XMVectorSubtract(face[3]->w, face[0]->w), normal));
		if (!is_flat && (origin_side == 0.0f || origin_side * opposite_side > 0.0f)) continue;
		const GjkClosest reduced = GetClosestOnTriangle(*face[0], *face[1], *face[2]);
		const float distance_sq = XMVectorGetX(XMVector3LengthSq(GetClosestPoint(reduced)));
		if (distance_sq < best_sq) {
			best = reduced;
			best_sq = distance_sq;
		}
	}
	return best;
}

// 점 목록이 이루는 볼록 도형에서 direction 쪽으로 가장 먼 점
static XMVECTOR GetSupportPoint(std::span<const XMVECTOR> points, FXMVECTOR direction)
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

// 두 도형의 차에서 원점에 가장 가까운 점을 GJK 방식으로 좁혀 간다. 그 점까지 거리가 두 도형 사이 거리이고, 단체 점의 비율로 두 도형 위의 점을 구한다.
ConvexDistance GetConvexDistance(std::span<const XMVECTOR> first, std::span<const XMVECTOR> second)
{
	const GjkVertex start{ XMVectorSubtract(first[0], second[0]), first[0], second[0] };
	GjkClosest simplex{ { start }, { 1.0f }, 1 };
	XMVECTOR v = start.w;
	for (int iteration = 0; iteration < GJK_MAX_ITERATIONS; ++iteration) {
		const float v_sq = XMVectorGetX(XMVector3LengthSq(v));
		if (v_sq <= 1e-12f) break;
		const XMVECTOR a = GetSupportPoint(first, XMVectorNegate(v));
		const XMVECTOR b = GetSupportPoint(second, v);
		const XMVECTOR w = XMVectorSubtract(a, b);
		// 더 가까워지지 않으면 지금 거리가 최단 거리다.
		if (v_sq - XMVectorGetX(XMVector3Dot(v, w)) <= GJK_EPSILON * v_sq) break;
		simplex.vertices[simplex.count++] = { w, a, b };
		const std::array<GjkVertex, 4> p = simplex.vertices;
		if (simplex.count == 2) simplex = GetClosestOnSegment(p[0], p[1]);
		else if (simplex.count == 3) simplex = GetClosestOnTriangle(p[0], p[1], p[2]);
		else simplex = GetClosestOnTetrahedron(p[0], p[1], p[2], p[3]);
		if (simplex.count == 4) return { b, b, 0.0f };
		v = GetClosestPoint(simplex);
	}
	XMVECTOR first_point = XMVectorZero();
	XMVECTOR second_point = XMVectorZero();
	for (int i = 0; i < simplex.count; ++i) {
		first_point = XMVectorAdd(first_point, XMVectorScale(simplex.vertices[i].a, simplex.weights[i]));
		second_point = XMVectorAdd(second_point, XMVectorScale(simplex.vertices[i].b, simplex.weights[i]));
	}
	return { first_point, second_point, XMVectorGetX(XMVector3Length(v)) };
}

CapsuleSweep MakeCapsuleSweep(const MoveVector& start, const MoveVector& end, float radius, float half_height, bool is_ignoring_moving_out)
{
	const float axis_half = std::max(half_height - radius, 0.0f);
	CapsuleSweep sweep;
	sweep.start = start;
	sweep.end = end;
	sweep.radius = radius;
	sweep.is_ignoring_moving_out = is_ignoring_moving_out;
	sweep.axis[0] = XMVectorSet(start.x, start.y, start.z - axis_half, 0.0f);
	sweep.axis[1] = XMVectorSet(start.x, start.y, start.z + axis_half, 0.0f);
	sweep.delta = XMVectorSet(end.x - start.x, end.y - start.y, end.z - start.z, 0.0f);
	// 닿았다고 보는 거리만큼 넓혀 둔다.
	const float reach = radius + MOVE_SWEEP_CONTACT_DISTANCE;
	sweep.min = { std::min(start.x, end.x) - reach, std::min(start.y, end.y) - reach, std::min(start.z, end.z) - axis_half - reach };
	sweep.max = { std::max(start.x, end.x) + reach, std::max(start.y, end.y) + reach, std::max(start.z, end.z) + axis_half + reach };
	return sweep;
}

static MoveVector ToMoveVector(FXMVECTOR value)
{
	return { XMVectorGetX(value), XMVectorGetY(value), XMVectorGetZ(value) };
}

// 캡슐 축(양 끝 axis, 반지름 radius)이 면들로 둘러싼 도형에 깊이 파고들었을 때, 가장 적게 밀어 내는 면 법선과 거리.
// 면마다 캡슐 전체가 그 면 바깥으로 나가는 거리를 구해 가장 짧은 면을 고른다. 면이 없으면 위로 반지름만큼 밀어 낸다.
static std::pair<XMVECTOR, float> GetPlanesPenetration(std::span<const XMVECTOR> axis, float radius, std::span<const XMVECTOR> planes)
{
	XMVECTOR best_normal = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	float best_depth = planes.empty() ? radius : std::numeric_limits<float>::infinity();
	for (const XMVECTOR& plane : planes) {
		float inner = std::numeric_limits<float>::infinity();
		for (const XMVECTOR& point : axis) inner = std::min(inner, XMVectorGetX(XMVector3Dot(plane, point)));
		const float depth = XMVectorGetW(plane) + radius - inner;
		if (depth < best_depth) {
			best_normal = XMVectorSetW(plane, 0.0f);
			best_depth = depth;
		}
	}
	return { best_normal, best_depth };
}

// 캡슐 축을 옮겨 가며 도형까지 거리를 다시 재고, 남은 틈을 다가가는 속도로 나눈 만큼씩 나아간다(보수적 전진).
// 옮기는 동안 거리는 볼록하게 변하므로 이렇게 나아가면 닿는 때를 지나치지 않고, 다가가지 않게 되면 닿지 않는다.
// 큰 월드 좌표에서 오차가 생기지 않도록 시작 중심을 원점으로 옮겨 계산한다.
MoveHit SweepConvex(const CapsuleSweep& sweep, std::span<const XMVECTOR> shape, float shape_radius, std::span<const XMVECTOR> planes, bool is_single_face)
{
	MoveHit hit;
	hit.trace_start = sweep.start;
	hit.trace_end = sweep.end;
	if (shape.empty()) return hit;
	const XMVECTOR origin = XMVectorSet(sweep.start.x, sweep.start.y, sweep.start.z, 0.0f);
	thread_local std::vector<XMVECTOR> points;
	points.clear();
	for (const XMVECTOR& point : shape) points.push_back(XMVectorSubtract(point, origin));
	const XMVECTOR axis[2] = { XMVectorSubtract(sweep.axis[0], origin), XMVectorSubtract(sweep.axis[1], origin) };
	const float reach = sweep.radius + shape_radius;
	float time = 0.0f;
	ConvexDistance closest = GetConvexDistance(axis, points);
	const float start_gap = closest.distance - reach;
	for (int iteration = 1; ; ++iteration) {
		const bool has_direction = closest.distance > 1e-4f;
		const XMVECTOR normal = has_direction ? XMVectorScale(XMVectorSubtract(closest.first_point, closest.second_point), 1.0f / closest.distance) : XMVectorZero();
		const float closing = -XMVectorGetX(XMVector3Dot(sweep.delta, normal));
		if (closest.distance - reach <= MOVE_SWEEP_CONTACT_DISTANCE) {
			// 처음부터 닿아만 있고 멀어지거나 나란히 가면 막히지 않는다.
			if (iteration == 1 && start_gap >= -MOVE_SWEEP_CONTACT_DISTANCE && has_direction && closing <= 0.0f) return hit;
			break;
		}
		if (closing <= 0.0f) return hit;
		time += (closest.distance - reach) / closing;
		if (time > 1.0f) return hit;
		// 끝까지 좁혀지지 않으면 지금 자리에서 닿은 것으로 본다. 지나치지 않은 자리라 막는 쪽으로 틀린다.
		if (iteration >= GJK_MAX_ITERATIONS) break;
		const XMVECTOR moved[2] = { XMVectorAdd(axis[0], XMVectorScale(sweep.delta, time)), XMVectorAdd(axis[1], XMVectorScale(sweep.delta, time)) };
		closest = GetConvexDistance(moved, points);
	}

	hit.is_blocking = true;
	hit.time = time;
	const MoveVector delta = sweep.end - sweep.start;
	hit.location = sweep.start + delta * time;
	const bool has_normal = closest.distance > 1e-4f;
	const XMVECTOR normal = has_normal ? XMVectorScale(XMVectorSubtract(closest.first_point, closest.second_point), 1.0f / closest.distance) : XMVectorZero();
	const XMVECTOR impact_point = XMVectorAdd(XMVectorAdd(closest.second_point, XMVectorScale(normal, shape_radius)), origin);
	hit.impact_point = ToMoveVector(impact_point);
	if (start_gap < -MOVE_SWEEP_CONTACT_DISTANCE) {
		// 처음부터 겹쳤으면 빠져나올 방향과 거리를 둔다. 캡슐 축이 도형을 지나 방향을 알 수 없으면 면으로 구한다.
		hit.is_start_penetrating = true;
		hit.time = 0.0f;
		hit.location = sweep.start;
		if (has_normal) {
			hit.normal = ToMoveVector(normal);
			hit.penetration_depth = -start_gap;
		}
		else {
			const auto [plane_normal, depth] = GetPlanesPenetration(sweep.axis, reach, planes);
			hit.normal = ToMoveVector(plane_normal);
			hit.penetration_depth = depth;
		}
		hit.impact_normal = hit.normal;
		// 볼록 도형 하나에서 빠져나가는 중이면 그 도형에 다시 막히지 않는다.
		if (sweep.is_ignoring_moving_out && IsMovingOutOfPenetration(hit, delta)) return MoveHit{ false, false, 1.0f, sweep.start, sweep.end };
		return hit;
	}
	hit.normal = has_normal ? ToMoveVector(normal) : SafeNormal(-delta);
	hit.impact_normal = hit.normal;
	if (is_single_face) {
		hit.impact_normal = ToMoveVector(planes[0]);
		return hit;
	}
	// 닿은 점 가까이 있는 면 가운데 이동과 가장 맞서는 면이 닿은 면이다.
	float most_opposing = std::numeric_limits<float>::infinity();
	for (const XMVECTOR& plane : planes) {
		if (std::fabs(XMVectorGetX(XMVector3Dot(plane, impact_point)) - XMVectorGetW(plane)) >= MOVE_SWEEP_FACE_SEARCH_DISTANCE) continue;
		const float opposing = XMVectorGetX(XMVector3Dot(plane, sweep.delta));
		if (opposing < most_opposing) {
			most_opposing = opposing;
			hit.impact_normal = ToMoveVector(plane);
		}
	}
	return hit;
}

const MoveHit& GetFirstBlockingHit(const MoveHit& best, const MoveHit& hit, const MoveVector& delta)
{
	if (!hit.is_blocking) return best;
	if (!best.is_blocking) return hit;
	if (hit.is_start_penetrating || best.is_start_penetrating) {
		if (hit.is_start_penetrating != best.is_start_penetrating) return hit.is_start_penetrating ? hit : best;
		return Dot(hit.impact_normal, delta) < Dot(best.impact_normal, delta) ? hit : best;
	}
	// 두 삼각형이 만나는 모서리처럼 닿음 기준 거리 안에서 함께 닿았으면 같은 곳에 닿은 것으로 보고, 언리얼처럼 이동과 가장 맞서는 면(아래로 쓸면 윗면)을 고른다.
	if (std::fabs(hit.time - best.time) * Length(delta) <= MOVE_SWEEP_CONTACT_DISTANCE) return Dot(hit.impact_normal, delta) < Dot(best.impact_normal, delta) ? hit : best;
	return hit.time < best.time ? hit : best;
}
