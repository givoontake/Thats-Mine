#include "RoomState.h"

#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Flow/ClientStateSubsystem.h"
#include "Local/LocalSessionSubsystem.h"
#include "UI/Room/RoomWidget.h"
#include "Network/OnlineSessionSubsystem.h"

void URoomState::OnEnter()
{
	URoomWidget* room_widget = CastChecked<URoomWidget>(screen_);
	room_widget->on_leave_requested_.AddDynamic(this, &URoomState::HandleRoomLeave);
	room_widget->on_start_requested_.AddDynamic(this, &URoomState::HandleRoomStart);
	if (!controller_->GetGameInstance()->GetSubsystem<UClientStateSubsystem>()->IsLocalMode()) {
		room_widget->on_ready_requested_.AddDynamic(this, &URoomState::HandleReady);
		room_widget->on_kick_requested_.AddDynamic(this, &URoomState::HandleKick);
		controller_->GetGameInstance()->GetSubsystem<UOnlineSessionSubsystem>()->on_changed_.AddUObject(this, &URoomState::RefreshOnline);
		RefreshOnline();
		return;
	}
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
	controller_->GetGameInstance()->GetSubsystem<UOnlineSessionSubsystem>()->on_changed_.RemoveAll(this);
	URoomWidget* room_widget = CastChecked<URoomWidget>(screen_);
	room_widget->on_leave_requested_.RemoveAll(this);
	room_widget->on_start_requested_.RemoveAll(this);
	room_widget->on_ready_requested_.RemoveAll(this);
	room_widget->on_kick_requested_.RemoveAll(this);
}

void URoomState::HandleRoomLeave()
{
	UClientStateSubsystem* state = controller_->GetGameInstance()->GetSubsystem<UClientStateSubsystem>();
	if (!state->IsLocalMode()) { controller_->GetGameInstance()->GetSubsystem<UOnlineSessionSubsystem>()->LeaveRoom(); return; }
	if (state->GetState() != EClientState::ROOM)
	{
		return;
	}
	controller_->GetGameInstance()->GetSubsystem<ULocalSessionSubsystem>()->LeaveRoom();
	state->ChangeState(EClientState::LOBBY);
}

void URoomState::HandleRoomStart()
{
	if (!controller_->GetGameInstance()->GetSubsystem<UClientStateSubsystem>()->IsLocalMode()) { controller_->GetGameInstance()->GetSubsystem<UOnlineSessionSubsystem>()->StartGame(); return; }
	controller_->GetGameInstance()->GetSubsystem<ULocalSessionSubsystem>()->StartGame();
}

void URoomState::HandleReady(bool is_ready)
{
	controller_->GetGameInstance()->GetSubsystem<UOnlineSessionSubsystem>()->SetReady(is_ready);
}

void URoomState::HandleKick(int64 player_key)
{
	controller_->GetGameInstance()->GetSubsystem<UOnlineSessionSubsystem>()->KickPlayer(player_key);
}

void URoomState::RefreshOnline()
{
	if (!screen_) return;
	const auto* online = controller_->GetGameInstance()->GetSubsystem<UOnlineSessionSubsystem>();
	auto* widget = CastChecked<URoomWidget>(screen_);
	widget->SetRoom(online->GetRoom(), online->GetMembers(), online->GetPlayerKey(), online->CanStart());
	widget->ShowStatus(online->GetStatus());
	widget->SetPending(online->IsPending());
}
