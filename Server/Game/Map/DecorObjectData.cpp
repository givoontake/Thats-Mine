#include <algorithm>
#include <utility>
#include "DecorObjectData.h"
#include "MapFileReader.h"

using namespace DirectX;

bool DecorObjectData::Load(MapFileReader& reader)
{
	is_loaded_ = false;
	meshes_.assign(reader.ReadCount(), {});
	for (DecorMesh& mesh : meshes_) mesh.name = reader.ReadName();
	std::vector<std::pair<std::uint32_t, XMFLOAT3>> objects(reader.ReadCount());
	for (auto& [mesh, location] : objects) {
		reader.ReadValue(mesh);
		reader.ReadValue(location);
		if (mesh >= meshes_.size()) reader.Invalidate();
	}
	if (!reader.IsValid()) return false;

	// 위치를 메시 순서로 줄 세우고(같은 메시 안은 파일 순서), 메시마다 자기 구간의 시작 번호와 개수를 둔다.
	std::stable_sort(objects.begin(), objects.end(), [](const auto& first, const auto& second) { return first.first < second.first; });
	locations_.clear();
	locations_.reserve(objects.size());
	for (std::uint32_t index = 0; index < objects.size(); ++index) {
		DecorMesh& mesh = meshes_[objects[index].first];
		if (mesh.location_count == 0) mesh.first_location = index;
		++mesh.location_count;
		locations_.push_back(objects[index].second);
	}
	is_loaded_ = true;
	return true;
}

const DecorMesh* DecorObjectData::FindMesh(const std::string& name) const
{
	const auto found = std::find_if(meshes_.begin(), meshes_.end(), [&](const DecorMesh& mesh) { return mesh.name == name; });
	return found != meshes_.end() ? &*found : nullptr;
}
