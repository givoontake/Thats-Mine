#pragma once

#include "CoreMinimal.h"
#include "settings.h"
#include "lobby_room_info.generated.h"

USTRUCT(BlueprintType)
struct FLobbyRoomInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int64 room_key = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString room_name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 player_count = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 max_player_count = MAX_ROOM_PLAYER_COUNT;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool is_private = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool is_playing = false;
};
