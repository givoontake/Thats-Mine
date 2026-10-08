#pragma once
#include <string>
#include <vector>
#include <DirectXMath.h>
#include "CollisionObjectData.h"
#include "DecorObjectData.h"
#include "LandscapeData.h"

// 게임 맵의 고정 데이터(랜드스케이프, 충돌 배치물, 충돌 없는 물체)를 서버 시작 때 맵 데이터 파일에서 읽어 둔다.
// 읽은 뒤에는 바뀌지 않으므로 여러 방 스레드가 잠금 없이 같이 읽는다.
class MapData
{
	LandscapeData landscape_;
	CollisionObjectData collision_objects_;
	DecorObjectData decor_objects_;
	bool is_loaded_ = false;

public:
	// tower_candidates: 탑 후보마다 탑 바닥 가운데 위치. 후보마다 탑과 기초 배치를 넣는다.
	bool Load(const std::string& file_path, const std::vector<DirectX::XMFLOAT3>& tower_candidates);
	bool IsLoaded() const { return is_loaded_; }
	const LandscapeData& GetLandscape() const { return landscape_; }
	const CollisionObjectData& GetCollisionObjects() const { return collision_objects_; }
	const DecorObjectData& GetDecorObjects() const { return decor_objects_; }
};
