#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "weapon_type.h"
#include "shot_result.generated.h"

USTRUCT(BlueprintType)
struct FShotResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	EWeaponType weapon = EWeaponType::SNIPER;

	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	FVector muzzle_location = FVector::ZeroVector;

	// 탄 광선 시작점. 캡슐 안 오른쪽 어깨 높이 고정점이다.
	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	FVector trace_start = FVector::ZeroVector;

	// 조준점. 카메라 시선이 처음 닿은 곳이고, 닿지 않았으면 사거리 끝이다. has_hit과 hit은 그 시선 검사 결과다.
	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	FVector trace_end = FVector::ZeroVector;

	// 발사할 때 카메라 위치. 서버가 카메라가 있을 수 있는 자리인지, 맞은 곳이 보였는지 확인한다.
	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	FVector camera_location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	bool has_hit = false;

	// 퍼지기 전 조준 방향(시작점에서 조준점 쪽). 서버가 이 방향에 빗나감을 적용해 판정한다.
	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	FVector aim_direction = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	FHitResult hit;
};
