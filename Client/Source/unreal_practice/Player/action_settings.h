#pragma once

#include "CoreMinimal.h"
#include "action_settings.generated.h"

class UAnimMontage;

USTRUCT(BlueprintType)
struct FActionSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	TObjectPtr<UAnimMontage> montage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action", meta = (ClampMin = "0.01"))
	float play_rate = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Action")
	bool should_disable_movement = false;
};
