#pragma once
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <utility>
#include <DirectXMath.h>

// 랜드스케이프와 충돌 배치물 판정이 같이 쓰는 기하 함수

constexpr float PARALLEL_EPSILON = 1e-8f;

float GetAxis(const DirectX::XMFLOAT3& value, int axis);
void ExpandBounds(DirectX::XMFLOAT3& min, DirectX::XMFLOAT3& max, const DirectX::XMFLOAT3& point);
std::pair<DirectX::XMFLOAT3, DirectX::XMFLOAT3> TransformBounds(const DirectX::XMFLOAT3& min, const DirectX::XMFLOAT3& max, DirectX::FXMMATRIX matrix);
bool SegmentHitsBox(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, const DirectX::XMFLOAT3& min, const DirectX::XMFLOAT3& max);
bool SegmentHitsTriangle(DirectX::FXMVECTOR start, DirectX::FXMVECTOR segment, DirectX::FXMVECTOR a, DirectX::GXMVECTOR b, DirectX::HXMVECTOR c);
std::pair<float, float> ClipSegment2D(float x0, float y0, float x1, float y1, float min_x, float min_y, float max_x, float max_y);

// 칸 크기 1인 격자 위 선분 (x0, y0)→(x1, y1)이 지나는 칸을 시작점 쪽부터 차례로 visit에 넘긴다(격자 순회).
// visit이 true를 돌려주면 그 자리에서 멈추고 true를 돌려준다. 두 점은 [0, columns]×[0, rows] 안에 있어야 한다.
template <typename Visit>
bool WalkCells(float x0, float y0, float x1, float y1, int columns, int rows, Visit visit)
{
	constexpr float INF = std::numeric_limits<float>::infinity();
	int x = std::clamp(static_cast<int>(std::floor(x0)), 0, columns - 1);
	int y = std::clamp(static_cast<int>(std::floor(y0)), 0, rows - 1);
	const int end_x = std::clamp(static_cast<int>(std::floor(x1)), 0, columns - 1);
	const int end_y = std::clamp(static_cast<int>(std::floor(y1)), 0, rows - 1);
	const float dx = x1 - x0;
	const float dy = y1 - y0;
	const int step_x = dx > 0.0f ? 1 : (dx < 0.0f ? -1 : 0);
	const int step_y = dy > 0.0f ? 1 : (dy < 0.0f ? -1 : 0);
	const float delta_x = step_x != 0 ? 1.0f / std::fabs(dx) : INF;
	const float delta_y = step_y != 0 ? 1.0f / std::fabs(dy) : INF;
	float next_x = step_x > 0 ? (x + 1 - x0) * delta_x : (step_x < 0 ? (x0 - x) * delta_x : INF);
	float next_y = step_y > 0 ? (y + 1 - y0) * delta_y : (step_y < 0 ? (y0 - y) * delta_y : INF);
	// 지나는 칸은 가로·세로 칸 수의 합을 넘지 않는다.
	for (int guard = std::abs(end_x - x) + std::abs(end_y - y) + 2; guard > 0; --guard) {
		if (visit(x, y)) return true;
		if (x == end_x && y == end_y) return false;
		if (next_x < next_y) {
			x += step_x;
			next_x += delta_x;
		}
		else {
			y += step_y;
			next_y += delta_y;
		}
		if (x < 0 || y < 0 || x >= columns || y >= rows) return false;
	}
	return false;
}
