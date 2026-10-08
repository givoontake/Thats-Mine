#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "terrain_types.h"
#include "platform_layout.h"

// 게임 맵의 고정 지형(랜드스케이프 높이 격자, 건물·나무·바위 충돌)을 서버 시작 때 읽어 두고 선분 막힘과 땅 높이를 답한다.
// 게임마다 놓이는 자리가 다른 발판·탑은 모든 자리를 후보 배치물로 따로 만들어 두고, 검사할 때는 방이 넘긴 놓인 후보 번호 목록만 본다.
// 읽은 뒤에는 바뀌지 않으므로 여러 방 스레드가 잠금 없이 같이 읽는다.
class TerrainData
{
	std::vector<TerrainSource> sources_;
	std::vector<TerrainObject> objects_;
	// 판마다 놓이는 배치물 후보. 발판 자리 순서대로 발판, 그 뒤에 탑 후보마다 탑과 기초가 이어진다. 공간 격자에는 넣지 않는다.
	std::vector<TerrainObject> candidate_objects_;
	std::uint32_t first_tower_object_ = 0; // 탑 후보 i의 탑은 candidate_objects_[first_tower_object_ + 2i], 기초는 그 다음이다.
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
	// tower_candidates: 탑 후보마다 탑 바닥 가운데 위치. 후보마다 탑과 기초 배치를 넣는다.
	bool Load(const std::string& file_path, const std::vector<DirectX::XMFLOAT3>& tower_candidates);
	bool IsLoaded() const { return is_loaded_; }
	// 이번 게임에 놓인 발판·탑의 후보 배치물 번호 목록을 만든다. 방이 게임 시작 때 한 번 만들어 검사 함수에 넘긴다.
	std::vector<std::uint32_t> CollectPlacedObjects(const PlatformLayout& platform_layout, const std::vector<std::size_t>& tower_indices) const;
	// 선분이 flag 채널을 막는 배치물(건물·나무·바위·경계 볼륨·발판·탑)에 닿는지 본다. 좌표는 언리얼 월드 좌표(cm)다.
	// 발판·탑은 placed_objects(놓인 후보 배치물 번호)만 본다.
	bool IsSegmentBlockedByObjects(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, std::uint8_t flag, const std::vector<std::uint32_t>& placed_objects) const;
	// 세로로 선 캡슐이 flag 채널을 막는 배치물과 겹치는지 본다. center는 캡슐 중심, half_height는 중심에서 위·아래 끝까지 거리(양 끝 반구 포함)다.
	// 발판·탑은 placed_objects(놓인 후보 배치물 번호)만 본다.
	bool IsCapsuleOverlappingObjects(const DirectX::XMFLOAT3& center, float radius, float half_height, std::uint8_t flag, const std::vector<std::uint32_t>& placed_objects) const;
	// 선분이 랜드스케이프에 닿는지 본다.
	bool IsSegmentBlockedByLandscape(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end) const;
	// (x, y) 지점의 랜드스케이프 높이. 랜드스케이프 밖이거나 구멍이면 값이 없다.
	std::optional<float> GetGroundHeight(float x, float y) const;
};
