#pragma once

#include "CoreMinimal.h"
#include "inventory_hint.generated.h"

UENUM(BlueprintType)
enum class EHintGrade : uint8
{
	S,
	A,
	B,
	C
};

USTRUCT(BlueprintType)
struct FInventoryHint
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EHintGrade grade = EHintGrade::C;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText text;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool is_new = true;
};
