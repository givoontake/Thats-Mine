#include <cstring>
#include "MapFileReader.h"

using namespace DirectX;

XMMATRIX MapTransform::ToMatrix() const
{
	return XMMatrixAffineTransformation(XMLoadFloat3(&scale), XMVectorZero(), XMLoadFloat4(&rotation), XMLoadFloat3(&position));
}

// 파일 끝을 넘어 읽으면 실패로 끝낸다.
void MapFileReader::Read(void* destination, size_t size)
{
	if (!is_valid_ || offset_ + size > bytes_.size()) {
		is_valid_ = false;
		return;
	}
	std::memcpy(destination, bytes_.data() + offset_, size);
	offset_ += size;
}

bool MapFileReader::ReadHeader(const char* magic, std::uint32_t version)
{
	char file_magic[4] = {};
	std::uint32_t file_version = 0;
	Read(file_magic, sizeof(file_magic));
	ReadValue(file_version);
	if (std::memcmp(file_magic, magic, sizeof(file_magic)) != 0 || file_version != version) is_valid_ = false;
	return is_valid_;
}

std::uint32_t MapFileReader::ReadCount()
{
	std::uint32_t count = 0;
	ReadValue(count);
	// 남은 파일보다 많은 개수는 잘못된 파일이다.
	if (count > bytes_.size()) is_valid_ = false;
	return is_valid_ ? count : 0u;
}

std::string MapFileReader::ReadName()
{
	char name[MAP_NAME_SIZE] = {};
	Read(name, sizeof(name));
	name[MAP_NAME_SIZE - 1] = '\0';
	return name;
}

MapTransform MapFileReader::ReadTransform()
{
	MapTransform transform{};
	ReadValue(transform.position);
	ReadValue(transform.rotation);
	ReadValue(transform.scale);
	return transform;
}

void MapFileReader::Skip(size_t size)
{
	if (!is_valid_ || offset_ + size > bytes_.size()) {
		is_valid_ = false;
		return;
	}
	offset_ += size;
}
