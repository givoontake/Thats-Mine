#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <DirectXMath.h>

class MapFileReader;

// 충돌 없는 물체의 메시 하나. 이 메시 물체의 위치는 위치 목록의 first_location번부터 location_count개다.
struct DecorMesh {
	std::string name;
	std::uint32_t first_location = 0;
	std::uint32_t location_count = 0;
};

// 게임 맵의 충돌 없는 물체(힌트에 쓰는 데이지·버섯 등) 위치를 갖는다. 위치는 메시 순서로 줄 세워 메시마다 자기 구간을 갖는다.
// 읽은 뒤에는 바뀌지 않으므로 여러 방 스레드가 잠금 없이 같이 읽는다.
class DecorObjectData
{
	std::vector<DecorMesh> meshes_;
	std::vector<DirectX::XMFLOAT3> locations_; // 물체 바닥 위치(cm)
	bool is_loaded_ = false;

public:
	// 맵 파일의 충돌 없는 물체 부분을 읽는다.
	bool Load(MapFileReader& reader);
	bool IsLoaded() const { return is_loaded_; }
	// 이름이 name인 메시. 없으면 nullptr이다.
	const DecorMesh* FindMesh(const std::string& name) const;
	const std::vector<DirectX::XMFLOAT3>& GetLocations() const { return locations_; }
};
