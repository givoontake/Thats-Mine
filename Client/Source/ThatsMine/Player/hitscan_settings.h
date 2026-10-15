#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "hitscan_settings.generated.h"

class UAnimMontage;
class UNiagaraSystem;
class USoundBase;

USTRUCT(BlueprintType)
struct FHitscanSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitscan")
	bool is_enabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitscan", meta = (EditCondition = "is_enabled", ClampMin = "1", Units = "cm"))
	float distance = 10000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitscan", meta = (EditCondition = "is_enabled", ClampMin = "0.01", Units = "s"))
	float fire_interval = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitscan", meta = (EditCondition = "is_enabled", ClampMin = "1"))
	int32 magazine_capacity = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitscan", meta = (EditCondition = "is_enabled", ClampMin = "0.01", Units = "s"))
	float reload_duration = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitscan", meta = (EditCondition = "is_enabled"))
	TObjectPtr<UAnimMontage> reload_montage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitscan", meta = (EditCondition = "is_enabled"))
	FName muzzle_socket = TEXT("Muzzle");

	// 발사할 때 총구 소켓에 붙여 재생한다. 쓰기 전까지 불러오지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitscan", meta = (EditCondition = "is_enabled"))
	TSoftObjectPtr<UNiagaraSystem> muzzle_effect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitscan", meta = (EditCondition = "is_enabled", ClampMin = "0.01"))
	float muzzle_effect_scale = 1.0f;

	// 발사할 때 총구에서 재생한다. 사거리 안에서만 들리고 멀어질수록 작아진다. 쓰기 전까지 불러오지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitscan", meta = (EditCondition = "is_enabled"))
	TSoftObjectPtr<USoundBase> fire_sound;

	// 재장전을 시작할 때 캐릭터에 붙여 재생한다. 재장전 시간과 같은 길이로 만든다. 쓰기 전까지 불러오지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitscan", meta = (EditCondition = "is_enabled"))
	TSoftObjectPtr<USoundBase> reload_sound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitscan", meta = (EditCondition = "is_enabled", Units = "cm"))
	float body_height = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hitscan", meta = (EditCondition = "is_enabled"))
	TEnumAsByte<ECollisionChannel> trace_channel = ECC_Visibility;
};
