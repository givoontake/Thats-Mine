#pragma once

#include "CoreMinimal.h"
#include "Login/LoginGameMode.h"
#include "LobbyGameMode.generated.h"

UCLASS()
class UNREAL_PRACTICE_API ALobbyGameMode : public ALoginGameMode
{
	GENERATED_BODY()

public:
	ALobbyGameMode();
};
