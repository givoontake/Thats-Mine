#include "map_geometry.h"

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
