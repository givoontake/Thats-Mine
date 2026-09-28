#pragma once

#include "CoreMinimal.h"
#include "UI/Lobby/lobby_room_info.h"

struct FLocalRoom
{
	FLobbyRoomInfo info;
	FString password;
	bool is_sample = false;
};

struct FLocalRoomResult
{
	bool is_success = false;
	FText message;
	FLobbyRoomInfo room;
};
