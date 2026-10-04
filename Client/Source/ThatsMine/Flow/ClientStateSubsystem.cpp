#include "ClientStateSubsystem.h"

#include "Engine/GameInstance.h"
#include "Local/LocalSessionSubsystem.h"
#include "Network/GameNetworkSubsystem.h"

void UClientStateSubsystem::ChangeState(EClientState state)
{
	if (state_ == state)
	{
		return;
	}
	state_ = state;
	on_state_changed_.Broadcast(state_);
}

void UClientStateSubsystem::StartLocalSession()
{
	is_local_mode_ = true;
	GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>()->Disconnect();
	GetGameInstance()->GetSubsystem<ULocalSessionSubsystem>()->ResetSession();
	GetGameInstance()->GetSubsystem<ULocalSessionSubsystem>()->StartGame();
}