#include <algorithm>
#include <utility>
#include "DecorObjectData.h"
#include "MapFileReader.h"

using namespace DirectX;

bool DecorObjectData::Load(const std::vector<MapMeshFile>& meshes)
{
	is_loaded_ = false;
	meshes_.clear();
	locations_.clear();
	// 위치를 항목 순서로 이어 담아(같은 항목 안은 파일 순서), 메시마다 자기 구간의 시작 번호와 개수를 둔다. 앞의 충돌 배치 부분은 건너뛴다.
	for (const MapMeshFile& file : meshes) {
		if (file.placements.empty()) continue;
		MapFileReader reader(file.placements);
		if (!reader.ReadHeader("TMPL", MAP_PLACEMENT_FILE_VERSION) || reader.ReadName() != file.name) return false;
		reader.Skip(static_cast<size_t>(reader.ReadCount()) * (sizeof(std::uint8_t) + sizeof(MapTransform)));
		const std::uint32_t count = reader.ReadCount();
		if (count > 0) {
			meshes_.push_back({ file.name, static_cast<std::uint32_t>(locations_.size()), count });
			for (std::uint32_t index = 0; index < count; ++index) reader.ReadValue(locations_.emplace_back());
		}
		if (!reader.IsValid() || !reader.IsAtEnd()) return false;
	}
	locations_.shrink_to_fit();
	is_loaded_ = true;
	return true;
}

const DecorMesh* DecorObjectData::FindMesh(const std::string& name) const
{
	const auto found = std::find_if(meshes_.begin(), meshes_.end(), [&](const DecorMesh& mesh) { return mesh.name == name; });
	return found != meshes_.end() ? &*found : nullptr;
}
