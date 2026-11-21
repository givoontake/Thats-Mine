#pragma once
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <span>
#include <utility>
#include <DirectXMath.h>
#include "shared_movement.h"

// 랜드스케이프와 충돌 배치물 판정이 같이 쓰는 기하 함수

constexpr float PARALLEL_EPSILON = 1e-8f;
// GJK 반복 한도와, 더 가까워지지 않는다고 볼 비율
constexpr int GJK_MAX_ITERATIONS = 32;
constexpr float GJK_EPSILON = 1e-6f;
// GJK 사면체 부피가 세 변 길이 곱의 이 비율 이하면 네 점이 한 평면에 있다고 본다.
constexpr float GJK_FLAT_RATIO = 1e-4f;

float GetAxis(const DirectX::XMFLOAT3& value, int axis);
void ExpandBounds(DirectX::XMFLOAT3& min, DirectX::XMFLOAT3& max, const DirectX::XMFLOAT3& point);
std::pair<DirectX::XMFLOAT3, DirectX::XMFLOAT3> TransformBounds(const DirectX::XMFLOAT3& min, const DirectX::XMFLOAT3& max, DirectX::FXMMATRIX matrix);
bool SegmentHitsBox(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, const DirectX::XMFLOAT3& min, const DirectX::XMFLOAT3& max);
bool SegmentHitsTriangle(DirectX::FXMVECTOR start, DirectX::FXMVECTOR segment, DirectX::FXMVECTOR a, DirectX::GXMVECTOR b, DirectX::HXMVECTOR c);
std::pair<float, float> ClipSegment2D(float x0, float y0, float x1, float y1, float min_x, float min_y, float max_x, float max_y);
bool BoxesOverlap(const DirectX::XMFLOAT3& first_min, const DirectX::XMFLOAT3& first_max, const DirectX::XMFLOAT3& second_min, const DirectX::XMFLOAT3& second_max);

// 볼록 도형 두 개(꼭짓점 목록) 사이 최단 거리와, 그때 두 도형 위에서 서로 가장 가까운 점
struct ConvexDistance {
	DirectX::XMVECTOR first_point;
	DirectX::XMVECTOR second_point;
	float distance;
};
ConvexDistance GetConvexDistance(std::span<const DirectX::XMVECTOR> first, std::span<const DirectX::XMVECTOR> second);

// 세로로 선 캡슐을 start 중심에서 end 중심까지 옮기는 쓸기. axis는 시작 위치의 캡슐 축 아래·위 끝(양 끝 반구 중심)이고,
// min, max는 쓸고 지나가는 범위 전체를 감싸는 월드 상자다. 반지름이 0이고 반 높이가 0이면 선분 검사다.
// is_ignoring_moving_out이면 처음부터 겹친 도형 가운데 이동이 빠져나가는 쪽인 것은 막지 않은 것으로 본다. 언리얼 캐릭터 이동의 일반 이동과 같다.
struct CapsuleSweep {
	DirectX::XMVECTOR axis[2];
	DirectX::XMVECTOR delta;
	MoveVector start;
	MoveVector end;
	float radius;
	DirectX::XMFLOAT3 min;
	DirectX::XMFLOAT3 max;
	bool is_ignoring_moving_out;
};
CapsuleSweep MakeCapsuleSweep(const MoveVector& start, const MoveVector& end, float radius, float half_height, bool is_ignoring_moving_out);

// 캡슐 쓸기가 볼록 도형 하나(꼭짓점 shape, 겉 두께 shape_radius)에 처음 닿는 곳. 막히지 않으면 is_blocking이 아니다.
// planes(xyz는 바깥쪽 법선, w는 원점에서 면까지 거리)는 닿은 면 법선을 고르고 깊이 파고든 캡슐을 밀어 낼 면으로 쓴다.
// is_single_face면 planes[0] 하나가 삼각형 면이고 닿은 면 법선은 늘 그 법선이다. planes가 비면 닿은 면 법선은 닿은 점의 법선과 같다.
MoveHit SweepConvex(const CapsuleSweep& sweep, std::span<const DirectX::XMVECTOR> shape, float shape_radius, std::span<const DirectX::XMVECTOR> planes, bool is_single_face);

// 쓸기 결과 hit이 best보다 먼저 막으면 hit을 돌려준다. 처음부터 겹친 결과끼리와, 닿음 기준 거리 안에서 함께 닿은 결과끼리는 닿은 면이 이동과 가장 맞서는 쪽을 고른다.
const MoveHit& GetFirstBlockingHit(const MoveHit& best, const MoveHit& hit, const MoveVector& delta);

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
