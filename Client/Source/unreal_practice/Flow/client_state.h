#pragma once

#include "CoreMinimal.h"
#include "client_state.generated.h"

UENUM(BlueprintType)
enum class EClientState : uint8
{
	LOGIN,
	LOBBY,
	ROOM,
	LOADING,
	PLAYING
};
