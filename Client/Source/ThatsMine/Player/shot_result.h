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

	// 총구가 벽을 관통했으면 몸통 검사 시작점, 그 외에는 총구 위치다.
	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	FVector trace_start = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	FVector trace_end = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	bool is_muzzle_blocked = false;

	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	bool has_hit = false;

	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	FHitResult hit;
};
