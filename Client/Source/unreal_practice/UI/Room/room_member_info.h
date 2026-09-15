#pragma once

#include "CoreMinimal.h"
#include "room_member_info.generated.h"

USTRUCT(BlueprintType)
struct FRoomMemberInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int64 player_key = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString nickname;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool is_host = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool is_ready = false;
};
