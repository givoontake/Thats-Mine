#pragma once
#include "CoreMinimal.h"

class UWorld;

// 지형 풀 위치 계산 결과
struct FGrassLocations
{
	TMap<FString, TArray<FVector>> locations; // 메시 이름별 월드 위치(cm)
	TMap<FString, TArray<FTransform>> transforms; // 메시 이름별 월드 변환(위치·회전·크기). 할턴 배치 종류만 채운다.
	int32 invalid_component_count = 0; // 풀 맵이 없어 건너뛴 지형 조각 수
};

// 지형이 자동으로 까는 풀(지형 풀)의 메시 이름을 모은다. 밀도와 보이는 거리가 0보다 큰 종류만 넣는다.
TSet<FString> CollectGrassMeshNames(UWorld* world);

// 저장된 풀 맵(높이·가중치)으로, 엔진 배치 계산을 그대로 옮긴 코드로 mesh_names에 있는 지형 풀의 월드 위치와 변환을 구한다.
FGrassLocations ComputeGrassLocations(UWorld* world, const TSet<FString>& mesh_names);
