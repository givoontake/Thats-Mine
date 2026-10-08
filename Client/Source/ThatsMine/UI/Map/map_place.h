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

	// 배율 1에서의 글자 크기. 영역이 클수록 크게 하되, 편차가 크지 않게 14~28 사이로 정한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1"))
	float font_size = 22.0f;
};
