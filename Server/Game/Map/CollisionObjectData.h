#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "terrain_types.h"
#include "tower_types.h"

struct MapMeshFile;

// 게임 맵의 충돌 배치물(건물·나무·바위·경계 볼륨)을 갖고 선분 막힘과 캡슐 겹침을 답한다.
// 게임마다 놓이는 자리가 다른 탑은 모든 자리를 후보 배치물로 따로 만들어 두고, 검사할 때는 방이 넘긴 놓인 후보 번호 목록만 본다.
// 읽은 뒤에는 바뀌지 않으므로 여러 방 스레드가 잠금 없이 같이 읽는다.
class CollisionObjectData
{
	std::vector<TerrainSource> sources_;
	// 맵 배치. 원본 순서로 줄 세워 같은 메시 배치가 이어지게 두고, 원본마다 자기 구간(시작 번호·개수)을 갖는다.
	std::vector<TerrainObject> objects_;
	// 판마다 놓이는 배치물 후보. 탑 후보마다 종류별 탑과 기초가 이어진다. 공간 격자에는 넣지 않는다.
	// 탑 후보 i의 종류 k 탑은 candidate_objects_[i * (TOWER_KIND_COUNT + 1) + k], 기초는 종류별 탑 다음이다.
	std::vector<TerrainObject> candidate_objects_;
	std::vector<TerrainWorldCapsule> object_capsules_;
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
	// 맵 폴더 항목들의 충돌 모양 파일과 배치 파일의 충돌 배치 부분을 읽는다. 항목 순서가 원본 순서다.
	// tower_candidates: 탑 후보마다 탑 바닥 가운데 위치. 후보마다 종류별 탑과 기초 배치를 넣는다.
	bool Load(const std::vector<MapMeshFile>& meshes, const std::vector<DirectX::XMFLOAT3>& tower_candidates);
	bool IsLoaded() const { return is_loaded_; }
	// 이름이 name인 충돌 모양 원본. 원본의 first_object, object_count가 그 메시 배치 구간이다. 없으면 nullptr이다.
	const TerrainSource* FindSource(const std::string& name) const;
	// 맵 배치의 월드 위치(메시 기준 원점 위치, cm)
	DirectX::XMFLOAT3 GetObjectLocation(std::uint32_t index) const;
	// 이번 게임에 놓인 탑의 후보 배치물 번호 목록을 만든다. 방이 게임 시작 때 한 번 만들어 검사 함수에 넘긴다.
	std::vector<std::uint32_t> CollectPlacedObjects(const std::vector<TowerPlacement>& tower_placements) const;
	// 선분이 flag 채널을 막는 배치물(건물·나무·바위·경계 볼륨·탑)에 닿는지 본다. 좌표는 언리얼 월드 좌표(cm)다.
	// 탑은 placed_objects(놓인 후보 배치물 번호)만 본다.
	bool IsSegmentBlockedByObjects(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end, std::uint8_t flag, const std::vector<std::uint32_t>& placed_objects) const;
	// 세로로 선 캡슐이 flag 채널을 막는 배치물과 겹치는지 본다. center는 캡슐 중심, half_height는 중심에서 위·아래 끝까지 거리(양 끝 반구 포함)다.
	// 탑은 placed_objects(놓인 후보 배치물 번호)만 본다.
	bool IsCapsuleOverlappingObjects(const DirectX::XMFLOAT3& center, float radius, float half_height, std::uint8_t flag, const std::vector<std::uint32_t>& placed_objects) const;
};
