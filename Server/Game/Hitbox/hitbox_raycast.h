#pragma once
#include <vector>
#include "hitbox_types.h"

// 두 선분 사이 최단 거리의 제곱. 지형의 캡슐 도형 검사에도 쓴다.
float GetSegmentDistanceSquared(DirectX::FXMVECTOR first_start, DirectX::FXMVECTOR first_end, DirectX::FXMVECTOR second_start, DirectX::GXMVECTOR second_end);

// 광선이 캐릭터의 1차 판정 캡슐에 닿는지 본다. 자세를 계산하기 전에 엉뚱한 명중을 걸러 낸다.
// 캡슐은 캐릭터 위치(location)와 좌우 방향(yaw, 도)에 맞춰 놓고 margin(cm)만큼 반지름을 키운다. direction은 단위 벡터이고, 거리는 cm다.
bool IsRayNearCharacter(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float max_distance, const DirectX::XMFLOAT3& location, float yaw, const BroadphaseCapsule& capsule, float margin);

// 광선이 월드 히트박스 도형들과 처음 만나는 곳을 찾는다. 시작점이 도형 안이면 거리 0으로 맞는다.
// margin(cm)만큼 구·캡슐 반지름과 상자 절반 크기를 키워 검사한다.
HitboxRayHit RaycastHitboxes(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float max_distance, const std::vector<HitboxShape>& shapes, float margin);
