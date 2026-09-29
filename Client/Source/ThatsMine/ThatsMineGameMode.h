// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ThatsMineGameMode.generated.h"

/**
 *  Simple GameMode for a first person game
 */
UCLASS(abstract)
class AThatsMineGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AThatsMineGameMode();
	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* new_player, AActor* start_spot) override;

private:
	TOptional<FTransform> GetStartDomeSpawn() const;
};



