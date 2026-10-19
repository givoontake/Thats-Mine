#pragma once

#include "CoreMinimal.h"
#include "Network/game_protocol.h"
#include "inventory_hint.generated.h"

UENUM(BlueprintType)
enum class EHintGrade : uint8
{
	S,
	A,
	B,
	C
};

// 힌트 분류. 위치 힌트는 지역을 기준으로 한 방향 힌트, 단서 힌트는 보물 주변 물체 개수 힌트다.
UENUM(BlueprintType)
enum class EHintCategory : uint8
{
	LOCATION,
	CLUE
};

USTRUCT(BlueprintType)
struct FInventoryHint
{
	GENERATED_BODY()

	// 서버 힌트 번호. 같은 힌트를 다시 받았는지와 잃은 힌트를 찾을 때 쓴다.
	uint16 hint_id = 0;
	EHintCategory category = EHintCategory::LOCATION;
	// 단서 힌트의 물체 종류(HintObjectKind), 반경 번호, 개수. 돋보기가 주변 개수와 맞춰 본다.
	uint8 object_kind = 0;
	uint8 radius = 0;
	uint8 count = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText text;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool is_new = true;

	// 즐겨찾기한 힌트는 인벤토리에서 위에 먼저 보인다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool is_favorite = false;
};
