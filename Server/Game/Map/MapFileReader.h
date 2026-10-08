#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <DirectXMath.h>

// 맵 데이터 파일의 이름 칸 크기(바이트, 끝 0 포함)
constexpr int MAP_NAME_SIZE = 64;

// 파일에 적힌 배치 변환(위치, 회전 쿼터니언, 크기 배율)
struct MapTransform {
	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT4 rotation;
	DirectX::XMFLOAT3 scale;

	DirectX::XMMATRIX ToMatrix() const;
};

// 맵 데이터 파일을 앞에서부터 차례로 읽는다. 파일 끝을 넘어 읽으면 그 뒤로는 실패 상태가 되어 아무것도 읽지 않는다.
// 맵 데이터가 부분(충돌 배치물, 랜드스케이프)마다 이 읽기 객체를 넘겨 자기 부분을 읽게 한다.
class MapFileReader
{
	const std::vector<char>& bytes_;
	size_t offset_ = 0;
	bool is_valid_ = true;

public:
	explicit MapFileReader(const std::vector<char>& bytes) : bytes_(bytes) {}

	void Read(void* destination, size_t size);
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
