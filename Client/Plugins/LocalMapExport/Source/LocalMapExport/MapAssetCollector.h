#pragma once
#include "CoreMinimal.h"

class UBodySetup;
class UStaticMesh;
class UWorld;
class ULandscapeHeightfieldCollisionComponent;

// 항목 종류. 같은 이름에 여러 종류가 섞일 수 있어 비트로 둔다.
constexpr uint8 MAP_ASSET_MESH = 1; // 액터에 놓인 정적 메시
constexpr uint8 MAP_ASSET_FOLIAGE = 2; // 칠한 폴리지
constexpr uint8 MAP_ASSET_COMPONENT = 4; // 메시 없는 충돌 부품(브러시 등). 액터 이름으로 묶는다.
constexpr uint8 MAP_ASSET_LANDSCAPE_GRASS = 8; // 지형이 자동으로 까는 풀

// 배치물이 막는 채널. 서버 맵 파일의 배치 플래그와 같은 값이다.
constexpr uint8 MAP_BLOCKS_VISIBILITY = 1; // 총알(보이는 채널)을 막는다.
constexpr uint8 MAP_BLOCKS_PAWN = 2; // 캐릭터 이동(폰 채널)을 막는다.
constexpr uint8 MAP_MOVABLE = 4; // 움직이는 부품

// 충돌 배치 하나
struct FMapObject
{
	uint8 flags = 0;
	FTransform transform;
};

// 충돌 모양 원본 하나와 그것을 쓰는 맵의 모든 충돌 배치. 정적 메시는 메시마다(액터·폴리지를 합쳐), 메시 없는 부품은 부품마다 하나다.
struct FMapCollision
{
	const UObject* key = nullptr; // 정적 메시 또는 부품
	FString name; // 메시 이름(메시 없는 부품은 액터 이름). 이름이 겹치면 경로 순서대로 _1, _2를 붙인다.
	FString path; // 메시 에셋 경로(메시 없는 부품은 부품 경로)
	UStaticMesh* static_mesh = nullptr;
	const UBodySetup* body = nullptr;
	uint8 kinds = 0;
	TArray<FMapObject> objects;
};

// 출력 폴더 하나(Meshes/이름)에 해당하는 항목. 충돌 모양과 충돌 배치, 충돌 없는 물체 위치를 함께 가진다.
// 충돌 배치는 충돌 모양 단위, 충돌 없는 물체는 폴리지 종류(액터는 메시) 단위로 묶고, 이름이 같으면 한 항목이 된다.
struct FMapEntry
{
	FString name;
	uint8 kinds = 0;
	int32 collision = INDEX_NONE; // 충돌 모양 번호. 충돌 배치가 없으면 INDEX_NONE
	TArray<FVector> decor_locations; // 막지 않는 배치의 위치(플레이 영역 안). 지형 풀은 AddGrassLocations로 채운다.
};

// 게임 레벨을 훑어 고정 지형 배치를 항목별로 모은다. 서버가 게임마다 놓는 상자·발판·흩어진 탑은 뺀다.
class FMapAssetCollector
{
	FBox2D play_area_;
	TArray<FMapCollision> collisions_;
	TMap<const UObject*, int32> collision_indices_;
	TArray<FMapEntry> entries_;
	TMap<FString, int32> entry_indices_;
	TArray<const ULandscapeHeightfieldCollisionComponent*> landscapes_;
	int32 skipped_no_shape_ = 0;
	int32 skipped_dynamic_ = 0;
	int32 skipped_towers_ = 0;
	int32 invalid_grass_component_count_ = 0;

	FMapEntry& FindOrAddEntry(const FString& name, uint8 kind);
	void AddDecorLocation(FMapEntry& entry, const FVector& location);
	void NameCollisions();

public:
	FMapAssetCollector(UWorld* world, const FBox2D& play_area);
	// 고른 지형 풀 항목의 위치를 계산해 넣는다.
	void AddGrassLocations(UWorld* world, const TSet<FString>& entry_names);

	const TArray<FMapEntry>& GetEntries() const { return entries_; }
	const TArray<FMapCollision>& GetCollisions() const { return collisions_; }
	const TArray<const ULandscapeHeightfieldCollisionComponent*>& GetLandscapes() const { return landscapes_; }
	int32 GetSkippedNoShape() const { return skipped_no_shape_; }
	int32 GetSkippedDynamic() const { return skipped_dynamic_; }
	int32 GetSkippedTowers() const { return skipped_towers_; }
	int32 GetInvalidGrassComponentCount() const { return invalid_grass_component_count_; }
};
