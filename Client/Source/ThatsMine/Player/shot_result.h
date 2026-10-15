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

	// 퍼지기 전 조준 방향과 이 발에 쓴 퍼짐 크기(도). 서버가 같은 빗나감을 다시 계산해 확인한다.
	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	FVector aim_direction = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	float spread = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	FHitResult hit;
};
