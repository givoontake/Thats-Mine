#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "lobby_room_info.h"
#include "LobbyRoomItem.generated.h"

UCLASS()
class THATSMINE_API ULobbyRoomItem : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FLobbyRoomInfo room_;
};
