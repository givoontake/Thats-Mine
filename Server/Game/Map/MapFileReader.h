#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <DirectXMath.h>

// 맵 데이터 파일의 이름 칸 크기(바이트, 끝 0 포함)
constexpr int MAP_NAME_SIZE = 64;
// 파일 형식 버전. 맵 데이터 추출 도구와 같아야 한다.
constexpr std::uint32_t MAP_LANDSCAPE_FILE_VERSION = 1;
constexpr std::uint32_t MAP_COLLISION_FILE_VERSION = 1;
constexpr std::uint32_t MAP_PLACEMENT_FILE_VERSION = 1;

// 맵 폴더(Meshes/이름)의 항목 하나. 충돌 모양 파일과 배치 파일 내용을 그대로 담고, 없는 파일은 비워 둔다.
struct MapMeshFile {
	std::string name;
	std::vector<char> collision;
	std::vector<char> placements;
};

// 파일에 적힌 배치 변환(위치, 회전 쿼터니언, 크기 배율)
struct MapTransform {
	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT4 rotation;
	DirectX::XMFLOAT3 scale;

	DirectX::XMMATRIX ToMatrix() const;
};

// 맵 데이터 파일을 앞에서부터 차례로 읽는다. 파일 끝을 넘어 읽으면 그 뒤로는 실패 상태가 되어 아무것도 읽지 않는다.
// 맵 데이터의 부분(충돌 배치물, 랜드스케이프, 충돌 없는 물체)이 자기 파일마다 이 읽기 객체를 만들어 읽는다.
class MapFileReader
{
	const std::vector<char>& bytes_;
	size_t offset_ = 0;
	bool is_valid_ = true;

public:
	explicit MapFileReader(const std::vector<char>& bytes) : bytes_(bytes) {}

	void Read(void* destination, size_t size);
	// 파일 머리(표지 4글자, 버전)를 읽고 기대한 값인지 본다. 다르면 실패 상태가 된다.
	bool ReadHeader(const char* magic, std::uint32_t version);
	template <typename T>
	void ReadValue(T& value) { Read(&value, sizeof(T)); }
	// 개수. 남은 파일보다 많은 개수는 잘못된 파일로 보고 0을 돌려준다.
	std::uint32_t ReadCount();
	std::string ReadName();
	MapTransform ReadTransform();
	void Skip(size_t size);
	void Invalidate() { is_valid_ = false; }
	bool IsValid() const { return is_valid_; }
	bool IsAtEnd() const { return offset_ == bytes_.size(); }
};
