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

	// 탄 광선 시작점. 오른손 위치이고, 오른손이 벽이나 다른 몸에 묻혔으면 몸통 검사 시작점이다.
	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	FVector trace_start = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	FVector trace_end = FVector::ZeroVector;

	// 퍼짐을 적용한 발사 방향(단위 벡터). 시작점이 맞은 몸 안이면 맞은 곳이 시작점과 같아지므로 방향은 따로 둔다.
	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	FVector shot_direction = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	bool is_hand_blocked = false;

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
