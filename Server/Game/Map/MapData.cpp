#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include "MapData.h"
#include "MapFileReader.h"

using namespace DirectX;

static std::vector<char> ReadFileBytes(const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::binary);
	return std::vector<char>((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

// 폴더 구성: Landscape.bin(랜드스케이프 충돌 조각), Meshes/항목 이름/collision.bin(충돌 모양), Meshes/항목 이름/placements.bin(충돌 배치, 충돌 없는 물체)
// 항목 파일을 이름 순서로 모두 메모리에 읽은 뒤 부분마다 자기 파일을 읽게 하고, 다 읽으면 파일 내용은 버린다.
bool MapData::Load(const std::string& map_dir)
{
	is_loaded_ = false;
	const std::filesystem::path root(map_dir);
	const std::vector<char> landscape_bytes = ReadFileBytes(root / "Landscape.bin");
	MapFileReader landscape_reader(landscape_bytes);
	if (!landscape_reader.ReadHeader("TMLS", MAP_LANDSCAPE_FILE_VERSION) || !landscape_.Load(landscape_reader) || !landscape_reader.IsAtEnd()) return false;

	std::error_code error;
	std::vector<MapMeshFile> meshes;
	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(root / "Meshes", error)) {
		if (!entry.is_directory()) continue;
		MapMeshFile& mesh = meshes.emplace_back();
		mesh.name = entry.path().filename().string();
		if (std::filesystem::exists(entry.path() / "collision.bin")) mesh.collision = ReadFileBytes(entry.path() / "collision.bin");
		if (std::filesystem::exists(entry.path() / "placements.bin")) mesh.placements = ReadFileBytes(entry.path() / "placements.bin");
	}
	if (error || meshes.empty()) return false;
	std::sort(meshes.begin(), meshes.end(), [](const MapMeshFile& first, const MapMeshFile& second) { return first.name < second.name; });
	if (!collision_objects_.Load(meshes)) return false;
	if (!decor_objects_.Load(meshes)) return false;
	is_loaded_ = true;
	return true;
}
