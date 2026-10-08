#include <cstring>
#include <fstream>
#include <iterator>
#include "MapData.h"
#include "MapFileReader.h"

using namespace DirectX;

// 파일 형식 버전. 맵 데이터 내보내기 도구와 같아야 한다.
constexpr std::uint32_t MAP_DATA_VERSION = 3;

// 파일 순서: 머리(표지, 버전), 배치 액터 종류 이름, 충돌 모양 원본, 배치, 랜드스케이프 충돌 조각, 충돌 없는 물체
bool MapData::Load(const std::string& file_path, const std::vector<XMFLOAT3>& tower_candidates)
{
	is_loaded_ = false;
	std::ifstream file(file_path, std::ios::binary);
	if (!file.is_open()) return false;
	const std::vector<char> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	MapFileReader reader(bytes);

	char magic[4] = {};
	reader.Read(magic, sizeof(magic));
	std::uint32_t version = 0;
	reader.ReadValue(version);
	if (!reader.IsValid() || std::memcmp(magic, "TMTR", 4) != 0 || version != MAP_DATA_VERSION) return false;
	// 배치 액터 종류 이름은 쓰지 않는다.
	reader.Skip(static_cast<size_t>(reader.ReadCount()) * MAP_NAME_SIZE);
	if (!collision_objects_.Load(reader, tower_candidates)) return false;
	if (!landscape_.Load(reader)) return false;
	if (!decor_objects_.Load(reader)) return false;
	if (!reader.IsValid() || !reader.IsAtEnd()) return false;
	is_loaded_ = true;
	return true;
}
