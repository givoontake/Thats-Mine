#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Network/game_protocol.h"
#include "settings.h"
#include "HintObjectSubsystem.generated.h"

// 힌트 물체 하나의 바닥 위치와 HintObjectKind 값
struct FHintObjectPoint
{
	FVector3f location;
	uint8 kind;
};

// 게임 레벨이 시작될 때 레벨의 폴리지와 배치물에서 힌트 물체(hint_meshes.h 이름)를 찾아 위치를 모아 둔다. 서버 힌트 물체 트리와 같은 물체다.
// 돋보기가 주변 물체 수를 서버 개수 힌트와 같은 기준(물체 바닥까지 3차원 거리)으로 센다. 화면에 그려지지 않은 물체도 센다.
// 맵 범위를 개수 반경 중 가장 큰 값 크기의 칸으로 나눠, 물체를 칸 순서로 한 배열에 두고 칸마다 시작 위치를 적어 둔다. 주변 3×3칸에서 찾는다.
UCLASS()
class THATSMINE_API UHintObjectSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& world) override;

	// location에서 거리 radius(cm, CELL_SIZE 이하) 안의 물체 수를 HintObjectKind마다 센다. DAISY·MUSHROOM 칸은 모든 색을 합친 수다.
	TArray<int32> CountKinds(const FVector& location, float radius) const;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type world_type) const override;

private:
	static_assert(HINT_OBJECT_COUNT_RADII[1] <= HINT_OBJECT_COUNT_RADII[0], "가장 큰 개수 반경이 첫 값이어야 한다.");
	static constexpr float CELL_SIZE = HINT_OBJECT_COUNT_RADII[0];
	static constexpr int32 GRID_X = static_cast<int32>((MAP_MAX[0] - MAP_MIN[0]) / CELL_SIZE) + 1;
	static constexpr int32 GRID_Y = static_cast<int32>((MAP_MAX[1] - MAP_MIN[1]) / CELL_SIZE) + 1;

	// 칸 순서(칸 번호 = 세로 칸 × GRID_X + 가로 칸)로 정렬한 힌트 물체
	TArray<FHintObjectPoint> points_;
	// 칸마다 points_에서 그 칸이 시작하는 위치. 마지막 값은 points_ 개수다.
	TArray<int32> cell_starts_;
};
