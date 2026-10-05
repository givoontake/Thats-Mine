#include <algorithm>
#include <cmath>
#include <limits>
#include "hitbox_raycast.h"

using namespace DirectX;

constexpr float NO_HIT = -1.0f;

// 원점 기준 반지름 radius 구와 광선이 처음 만나는 거리. 시작점이 안이면 0, 만나지 않으면 NO_HIT다.
static float RaycastSphere(FXMVECTOR origin, FXMVECTOR direction, FXMVECTOR center, float radius)
{
	const XMVECTOR offset = XMVectorSubtract(origin, center);
	const float b = XMVectorGetX(XMVector3Dot(offset, direction));
	const float c = XMVectorGetX(XMVector3Dot(offset, offset)) - radius * radius;
	if (c <= 0.0f) return 0.0f;
	const float discriminant = b * b - c;
	if (discriminant < 0.0f) return NO_HIT;
	const float distance = -b - std::sqrt(discriminant);
	return distance >= 0.0f ? distance : NO_HIT;
}

// 도형 기준 좌표의 캡슐(Z축 방향 원통 길이 length와 양 끝 반구)과 광선이 처음 만나는 거리
static float RaycastCapsule(FXMVECTOR origin, FXMVECTOR direction, float radius, float length)
{
	const float half = length * 0.5f;
	const float origin_z = std::clamp(XMVectorGetZ(origin), -half, half);
	if (XMVectorGetX(XMVector3LengthSq(XMVectorSubtract(origin, XMVectorSet(0.0f, 0.0f, origin_z, 0.0f)))) <= radius * radius) return 0.0f;

	float nearest = NO_HIT;
	const auto keep = [&](float distance) { if (distance >= 0.0f && (nearest < 0.0f || distance < nearest)) nearest = distance; };
	// 원통 옆면: XY 평면에서 반지름 radius 원과 만나는 거리 중 높이가 원통 안인 것
	const float ox = XMVectorGetX(origin), oy = XMVectorGetY(origin), oz = XMVectorGetZ(origin);
	const float dx = XMVectorGetX(direction), dy = XMVectorGetY(direction), dz = XMVectorGetZ(direction);
	const float a = dx * dx + dy * dy;
	if (a > 1e-8f) {
		const float b = ox * dx + oy * dy;
		const float c = ox * ox + oy * oy - radius * radius;
		const float discriminant = b * b - a * c;
		if (discriminant >= 0.0f) {
			const float distance = (-b - std::sqrt(discriminant)) / a;
			if (std::abs(oz + distance * dz) <= half) keep(distance);
		}
	}
	// 양 끝 반구
	keep(RaycastSphere(origin, direction, XMVectorSet(0.0f, 0.0f, half, 0.0f), radius));
	keep(RaycastSphere(origin, direction, XMVectorSet(0.0f, 0.0f, -half, 0.0f), radius));
	return nearest;
}

// 도형 기준 좌표의 상자(축마다 half_size)와 광선이 처음 만나는 거리. 축마다 들어가는 거리와 나가는 거리를 좁혀 간다.
static float RaycastBox(FXMVECTOR origin, FXMVECTOR direction, const XMFLOAT3& half_size)
{
	float enter = 0.0f;
	float exit = std::numeric_limits<float>::max();
	const float origins[3] = { XMVectorGetX(origin), XMVectorGetY(origin), XMVectorGetZ(origin) };
	const float directions[3] = { XMVectorGetX(direction), XMVectorGetY(direction), XMVectorGetZ(direction) };
	const float halves[3] = { half_size.x, half_size.y, half_size.z };
	for (int axis = 0; axis < 3; ++axis) {
		if (std::abs(directions[axis]) < 1e-8f) {
			if (std::abs(origins[axis]) > halves[axis]) return NO_HIT;
			continue;
		}
		const float first = (-halves[axis] - origins[axis]) / directions[axis];
		const float second = (halves[axis] - origins[axis]) / directions[axis];
		enter = std::max(enter, std::min(first, second));
		exit = std::min(exit, std::max(first, second));
		if (enter > exit) return NO_HIT;
	}
	return enter;
}

// 두 선분 사이 최단 거리의 제곱. 각 선분 위에서 서로 가장 가까운 점을 0~1 비율로 찾는다.
float GetSegmentDistanceSquared(FXMVECTOR first_start, FXMVECTOR first_end, FXMVECTOR second_start, GXMVECTOR second_end)
{
	const XMVECTOR first = XMVectorSubtract(first_end, first_start);
	const XMVECTOR second = XMVectorSubtract(second_end, second_start);
	const XMVECTOR between = XMVectorSubtract(first_start, second_start);
	const float a = XMVectorGetX(XMVector3Dot(first, first));
	const float e = XMVectorGetX(XMVector3Dot(second, second));
	const float f = XMVectorGetX(XMVector3Dot(second, between));
	float s = 0.0f;
	float t = 0.0f;
	if (a > 1e-8f && e > 1e-8f) {
		const float b = XMVectorGetX(XMVector3Dot(first, second));
		const float c = XMVectorGetX(XMVector3Dot(first, between));
		const float denominator = a * e - b * b;
		s = denominator > 1e-8f ? std::clamp((b * f - c * e) / denominator, 0.0f, 1.0f) : 0.0f;
		t = (b * s + f) / e;
		if (t < 0.0f) {
			t = 0.0f;
			s = std::clamp(-c / a, 0.0f, 1.0f);
		}
		else if (t > 1.0f) {
			t = 1.0f;
			s = std::clamp((b - c) / a, 0.0f, 1.0f);
		}
	}
	else if (a > 1e-8f) {
		s = std::clamp(-XMVectorGetX(XMVector3Dot(first, between)) / a, 0.0f, 1.0f);
	}
	else if (e > 1e-8f) {
		t = std::clamp(f / e, 0.0f, 1.0f);
	}
	const XMVECTOR gap = XMVectorSubtract(XMVectorAdd(first_start, XMVectorScale(first, s)), XMVectorAdd(second_start, XMVectorScale(second, t)));
	return XMVectorGetX(XMVector3Dot(gap, gap));
}

// 캡슐 축의 치우침을 좌우 방향으로 돌려 월드 축 선분을 만들고, 광선 선분과의 최단 거리가 반지름 이하인지 본다.
bool IsRayNearCharacter(const XMFLOAT3& origin, const XMFLOAT3& direction, float max_distance, const XMFLOAT3& location, float yaw, const BroadphaseCapsule& capsule, float margin)
{
	const float yaw_radians = XMConvertToRadians(yaw);
	const float x = location.x + capsule.offset_x * std::cos(yaw_radians) - capsule.offset_y * std::sin(yaw_radians);
	const float y = location.y + capsule.offset_x * std::sin(yaw_radians) + capsule.offset_y * std::cos(yaw_radians);
	const XMVECTOR ray_start = XMLoadFloat3(&origin);
	const XMVECTOR ray_end = XMVectorAdd(ray_start, XMVectorScale(XMLoadFloat3(&direction), max_distance));
	const float radius = capsule.radius + margin;
	return GetSegmentDistanceSquared(ray_start, ray_end, XMVectorSet(x, y, location.z + capsule.bottom_z, 0.0f), XMVectorSet(x, y, location.z + capsule.top_z, 0.0f)) <= radius * radius;
}

HitboxRayHit RaycastHitboxes(const XMFLOAT3& origin, const XMFLOAT3& direction, float max_distance, const std::vector<HitboxShape>& shapes, float margin)
{
	HitboxRayHit hit;
	const XMVECTOR world_origin = XMLoadFloat3(&origin);
	const XMVECTOR world_direction = XMLoadFloat3(&direction);
	for (const HitboxShape& shape : shapes) {
		// 광선을 도형 기준 좌표로 옮겨 검사한다. 회전만 하므로 거리는 그대로다.
		const XMVECTOR rotation = XMLoadFloat4(&shape.transform.rotation);
		const XMVECTOR inverse = XMQuaternionInverse(rotation);
		const XMVECTOR local_origin = XMVector3Rotate(XMVectorSubtract(world_origin, XMLoadFloat3(&shape.transform.position)), inverse);
		const XMVECTOR local_direction = XMVector3Rotate(world_direction, inverse);
		float distance = NO_HIT;
		switch (shape.type) {
		case HitboxShapeType::SPHERE: distance = RaycastSphere(local_origin, local_direction, XMVectorZero(), shape.radius + margin); break;
		case HitboxShapeType::CAPSULE: distance = RaycastCapsule(local_origin, local_direction, shape.radius + margin, shape.length); break;
		case HitboxShapeType::BOX: distance = RaycastBox(local_origin, local_direction, XMFLOAT3(shape.half_size.x + margin, shape.half_size.y + margin, shape.half_size.z + margin)); break;
		}
		if (distance < 0.0f || distance > max_distance || (hit.has_hit && distance >= hit.distance)) continue;
		hit.has_hit = true;
		hit.distance = distance;
		hit.part = shape.part;
	}
	return hit;
}
