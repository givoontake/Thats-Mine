#include "BoxShapeData.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

bool BoxShapeData::Load(const std::string& file_path)
{
	for (std::size_t kind = 0; kind < KIND_COUNT; ++kind) {
		closed_shapes_[kind].clear();
		open_shapes_[kind].clear();
		bounding_radii_[kind] = 0.0f;
	}
	is_loaded_ = false;
	std::ifstream file(file_path);
	if (!file) return false;

	std::string line;
	while (std::getline(file, line)) {
		if (line.empty() || line[0] == '#') continue;

		std::istringstream stream(line);
		std::string kind_text, state_text;
		DirectX::XMFLOAT3 center{};
		DirectX::XMFLOAT3 half_size{};
		DirectX::XMFLOAT3 rotation{};
		stream >> kind_text >> state_text >> center.x >> center.y >> center.z >> half_size.x >> half_size.y >> half_size.z >> rotation.x >> rotation.y >> rotation.z;
		if (!stream || (kind_text != "Treasure" && kind_text != "Gift") || (state_text != "Closed" && state_text != "Open")) return false;

		const BoxKind kind = kind_text == "Treasure" ? BoxKind::TREASURE : BoxKind::GIFT;
		HitboxShape shape{};
		shape.type = HitboxShapeType::BOX;
		shape.transform.position = center;
		shape.transform.rotation = ConvertUnrealRotation(rotation);
		shape.half_size = half_size;
		(state_text == "Closed" ? closed_shapes_ : open_shapes_)[static_cast<std::size_t>(kind)].push_back(shape);
		// 회전과 상관없이 중심에서 꼭짓점까지 거리가 절반 대각선이다.
		const float reach = std::hypot(center.x, center.y, center.z) + std::hypot(half_size.x, half_size.y, half_size.z);
		bounding_radii_[static_cast<std::size_t>(kind)] = std::max(bounding_radii_[static_cast<std::size_t>(kind)], reach);
	}
	is_loaded_ = !closed_shapes_[static_cast<std::size_t>(BoxKind::TREASURE)].empty() && !closed_shapes_[static_cast<std::size_t>(BoxKind::GIFT)].empty();
	return is_loaded_;
}

const std::vector<HitboxShape>& BoxShapeData::GetShapes(BoxKind kind, bool is_open) const
{
	return (is_open ? open_shapes_ : closed_shapes_)[static_cast<std::size_t>(kind)];
}
