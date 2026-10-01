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
	virtual bool PlayerCanRestart_Implementation(APlayerController* player) override;
	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* new_player, AActor* start_spot) override;
	// 스폰 좌표 아래 바닥에 캐릭터가 서는 위치를 구한다.
	FTransform FindStartTransform(const FTransform& spawn) const;
};



