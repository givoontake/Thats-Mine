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

// 힌트 분류. 위치 힌트는 지역을 기준으로 한 방향·보임 힌트, 단서 힌트는 보물 주변 물체 개수 힌트다.
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

	// 서버가 보낸 힌트 값. 분류와 돋보기 개수 비교에 쓴다.
	HINT_ENTRY entry{};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText text;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool is_new = true;

	// 즐겨찾기한 힌트는 인벤토리에서 위에 먼저 보인다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool is_favorite = false;
};

inline EHintCategory GetHintCategory(const HINT_ENTRY& entry)
{
	return entry.kind == static_cast<uint8>(HintKind::OBJECT_COUNT) ? EHintCategory::CLUE : EHintCategory::LOCATION;
}
