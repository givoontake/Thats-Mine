#pragma once
#include <array>
#include <cstddef>
#include <string>
#include <vector>
#include "box_types.h"
#include "hitbox_types.h"

// 상자 종류마다 탄을 막는 직육면체 모양(닫힘·열림)을 서버 시작 때 읽어 둔다. 모양의 위치·회전은 상자 바닥 가운데 기준 상자 좌표다.
// 읽은 뒤에는 바뀌지 않으므로 여러 방 스레드가 잠금 없이 같이 읽는다.
class BoxShapeData
{
	static constexpr std::size_t KIND_COUNT = 2;

	// BoxKind 순서의 닫힌 모양과 열린 모양. 열린 모양이 없는 종류는 열리면 사라진다.
	std::array<std::vector<HitboxShape>, KIND_COUNT> closed_shapes_;
	std::array<std::vector<HitboxShape>, KIND_COUNT> open_shapes_;
	// 종류마다 모든 모양을 감싸는 구의 반지름(cm). 구의 중심은 상자 바닥 가운데다.
	std::array<float, KIND_COUNT> bounding_radii_{};
	bool is_loaded_ = false;

public:
	// 줄: 종류 상태 중심x 중심y 중심z 절반x 절반y 절반z pitch yaw roll
	bool Load(const std::string& file_path);
	bool IsLoaded() const { return is_loaded_; }
	const std::vector<HitboxShape>& GetShapes(BoxKind kind, bool is_open) const;
	float GetBoundingRadius(BoxKind kind) const { return bounding_radii_[static_cast<std::size_t>(kind)]; }
};
