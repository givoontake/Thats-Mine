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

	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	FVector trace_end = FVector::ZeroVector;

	// 퍼짐을 적용한 발사 방향(단위 벡터). 시작점이 맞은 몸 안이면 맞은 곳이 시작점과 같아지므로 방향은 따로 둔다.
	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	FVector shot_direction = FVector::ZeroVector;

	// 발사할 때 카메라 위치. 서버가 카메라가 있을 수 있는 자리인지, 맞은 곳이 보였는지 확인한다.
	UPROPERTY(BlueprintReadOnly, Category = "Shot")
	FVector camera_location = FVector::ZeroVector;

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
