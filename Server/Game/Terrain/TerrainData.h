#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "terrain_types.h"
#include "platform_layout.h"

// 게임 맵의 고정 지형(랜드스케이프 높이 격자, 건물·나무·바위 충돌)을 서버 시작 때 읽어 두고 선분 막힘과 땅 높이를 답한다.
// 게임마다 놓이는 자리가 다른 발판은 모든 자리를 배치물로 넣어 두고, 검사할 때 받은 발판 배치에 없는 자리는 건너뛴다.
// 읽은 뒤에는 바뀌지 않으므로 여러 방 스레드가 잠금 없이 같이 읽는다.
class TerrainData
{
	std::vector<TerrainSource> sources_;
	std::vector<TerrainObject> objects_;
	std::vector<TerrainWorldCapsule> object_capsules_;
	std::vector<TerrainHeightfield> heightfields_;
	// 배치물 공간 격자. 칸마다 겹치는 배치 번호를 cell_objects_에 이어 담고, 칸 c의 목록은 cell_starts_[c]부터 cell_starts_[c + 1] 전까지다.
	// 격자보다 큰 배치물은 칸에 넣지 않고 large_objects_에 두어 매번 본다.
	float grid_min_x_ = 0.0f;
	float grid_min_y_ = 0.0f;
	int grid_columns_ = 0;
	int grid_rows_ = 0;
	std::vector<std::uint32_t> cell_starts_;
	std::vector<std::uint32_t> cell_objects_;
	std::vector<std::uint32_t> large_objects_;
	bool is_loaded_ = false;

	void BuildGrid();
	bool IsSegmentBlockedByObject(const TerrainObject& object, const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end) const;
	bool IsCapsuleOverlappingObject(const TerrainObject& object, DirectX::FXMVECTOR bottom, DirectX::FXMVECTOR top, float radius, const DirectX::XMFLOAT3& min, const DirectX::XMFLOAT3& max) const;

public:
	bool Load(const std::string& file_path);
	bool IsLoaded() const { return is_loaded_; }
	// 선분이 flag 채널을 막는 배치물(건물·나무·바위·경계 볼륨·발판)에 닿는지 본다. 좌표는 언리얼 월드 좌표(cm)다.
	// 발판은 platform_layout에서 놓인 자리만 본다.
	bool IsSegmentBlockedByObjects(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, std::uint8_t flag, const PlatformLayout& platform_layout) const;
	// 세로로 선 캡슐이 flag 채널을 막는 배치물과 겹치는지 본다. center는 캡슐 중심, half_height는 중심에서 위·아래 끝까지 거리(양 끝 반구 포함)다.
	// 발판은 platform_layout에서 놓인 자리만 본다.
	bool IsCapsuleOverlappingObjects(const DirectX::XMFLOAT3& center, float radius, float half_height, std::uint8_t flag, const PlatformLayout& platform_layout) const;
	// 선분이 랜드스케이프에 닿는지 본다.
	bool IsSegmentBlockedByLandscape(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end) const;
	// (x, y) 지점의 랜드스케이프 높이. 랜드스케이프 밖이거나 구멍이면 값이 없다.
	std::optional<float> GetGroundHeight(float x, float y) const;
};
