#pragma once

#include "CoreMinimal.h"
#include "network_connection_state.generated.h"

UENUM(BlueprintType)
enum class ENetworkConnectionState : uint8
{
	DISCONNECTED,
	CONNECTED,
	FAILED
};
