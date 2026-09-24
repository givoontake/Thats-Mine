#pragma once

#include "CoreMinimal.h"
#include "map_place.generated.h"

USTRUCT(BlueprintType)
struct FMapPlace
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "1"))
	FVector2D position = FVector2D::ZeroVector;
};
