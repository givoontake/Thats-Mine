#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Flow/client_state.h"
#include "LoginGameMode.generated.h"

UCLASS()
class THATSMINE_API ALoginGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ALoginGameMode();
	virtual void InitGame(const FString& map_name, const FString& options, FString& error_message) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Client State")
	EClientState initial_state_ = EClientState::LOGIN;
};
