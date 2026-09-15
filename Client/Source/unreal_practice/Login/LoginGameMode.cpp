#include "LoginGameMode.h"

#include "Engine/GameInstance.h"
#include "Flow/ClientStateSubsystem.h"
#include "LoginPlayerController.h"

ALoginGameMode::ALoginGameMode()
{
	PlayerControllerClass = ALoginPlayerController::StaticClass();
	DefaultPawnClass = nullptr;
	HUDClass = nullptr;
}

void ALoginGameMode::InitGame(const FString& map_name, const FString& options, FString& error_message)
{
	Super::InitGame(map_name, options, error_message);
	GetGameInstance()->GetSubsystem<UClientStateSubsystem>()->ChangeState(initial_state_);
}
