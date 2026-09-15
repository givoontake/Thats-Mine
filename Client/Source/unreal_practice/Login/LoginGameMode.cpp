#include "LoginGameMode.h"

#include "Engine/GameInstance.h"
#include "Flow/ClientStateSubsystem.h"
#include "LoginPlayerController.h"
#include "Network/OnlineSessionSubsystem.h"

ALoginGameMode::ALoginGameMode()
{
	PlayerControllerClass = ALoginPlayerController::StaticClass();
	DefaultPawnClass = nullptr;
	HUDClass = nullptr;
}

void ALoginGameMode::InitGame(const FString& map_name, const FString& options, FString& error_message)
{
	Super::InitGame(map_name, options, error_message);
	auto* state = GetGameInstance()->GetSubsystem<UClientStateSubsystem>();
	const bool can_enter_lobby = state->IsLocalMode() || GetGameInstance()->GetSubsystem<UOnlineSessionSubsystem>()->IsLoggedIn();
	state->ChangeState(initial_state_ == EClientState::LOBBY && !can_enter_lobby ? EClientState::LOGIN : initial_state_);
}
