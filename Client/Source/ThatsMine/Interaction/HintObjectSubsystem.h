#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Network/game_protocol.h"
#include "hint_object_file.h"
#include "HintObjectSubsystem.generated.h"

// 서버 힌트 물체 트리와 같은 힌트 물체 목록(Content/Data/hint_objects.bin)을 게임을 켤 때 한 번 읽어 둔다.
// 돋보기가 주변 물체 수를 서버 개수 힌트와 같은 기준(물체 바닥까지 3차원 거리)으로 센다. 화면에 그려지지 않은 물체도 센다.
// 물체는 CELL_SIZE 칸으로 나눠 두고, 반경이 칸 크기 이하인 범위만 주변 3×3칸에서 찾는다.
UCLASS()
class THATSMINE_API UHintObjectSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& collection) override;

	// location에서 거리 radius(cm, CELL_SIZE 이하) 안의 물체 수를 HintObjectKind마다 센다. DAISY·MUSHROOM 칸은 모든 색을 합친 수다.
	TArray<int32> CountKinds(const FVector& location, float radius) const;

private:
	static constexpr float CELL_SIZE = 5000.0f;

	// 칸 좌표마다 그 칸에 바닥이 있는 물체
	TMap<FIntPoint, TArray<HintObjectRecord>> cells_;
};
