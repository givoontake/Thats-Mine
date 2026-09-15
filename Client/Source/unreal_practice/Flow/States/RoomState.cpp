#include "RoomState.h"

#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Flow/ClientStateSubsystem.h"
#include "Local/LocalSessionSubsystem.h"
#include "UI/Room/RoomWidget.h"

void URoomState::OnEnter()
{
	if (!controller_->GetGameInstance()->GetSubsystem<UClientStateSubsystem>()->IsLocalMode())
	{
		return;
	}
	URoomWidget* room_widget = CastChecked<URoomWidget>(screen_);
	room_widget->on_leave_requested_.AddDynamic(this, &URoomState::HandleRoomLeave);
	room_widget->on_start_requested_.AddDynamic(this, &URoomState::HandleRoomStart);
	const TOptional<FLobbyRoomInfo> room = controller_->GetGameInstance()->GetSubsystem<ULocalSessionSubsystem>()->GetCurrentRoom();
	if (room.IsSet())
	{
		FRoomMemberInfo member;
		member.player_key = 1;
		member.nickname = TEXT("로컬 플레이어");
		member.is_host = true;
		room_widget->SetRoom(room.GetValue(), {member}, member.player_key, true);
	}
}

void URoomState::OnExit()
{
	URoomWidget* room_widget = CastChecked<URoomWidget>(screen_);
	room_widget->on_leave_requested_.RemoveAll(this);
	room_widget->on_start_requested_.RemoveAll(this);
}

void URoomState::HandleRoomLeave()
{
	UClientStateSubsystem* state = controller_->GetGameInstance()->GetSubsystem<UClientStateSubsystem>();
	if (!state->IsLocalMode() || state->GetState() != EClientState::ROOM)
	{
		return;
	}
	controller_->GetGameInstance()->GetSubsystem<ULocalSessionSubsystem>()->LeaveRoom();
	state->ChangeState(EClientState::LOBBY);
}

void URoomState::HandleRoomStart()
{
	controller_->GetGameInstance()->GetSubsystem<ULocalSessionSubsystem>()->StartGame();
}
