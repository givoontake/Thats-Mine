#include "RoomState.h"

#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "UI/Room/RoomWidget.h"
#include "Network/OnlineSessionSubsystem.h"

void URoomState::OnEnter()
{
	URoomWidget* room_widget = CastChecked<URoomWidget>(screen_);
	room_widget->on_leave_requested_.AddDynamic(this, &URoomState::HandleRoomLeave);
	room_widget->on_start_requested_.AddDynamic(this, &URoomState::HandleRoomStart);
	room_widget->on_ready_requested_.AddDynamic(this, &URoomState::HandleReady);
	room_widget->on_kick_requested_.AddDynamic(this, &URoomState::HandleKick);
	controller_->GetGameInstance()->GetSubsystem<UOnlineSessionSubsystem>()->on_changed_.AddUObject(this, &URoomState::RefreshOnline);
	RefreshOnline();
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
	controller_->GetGameInstance()->GetSubsystem<UOnlineSessionSubsystem>()->LeaveRoom();
}

void URoomState::HandleRoomStart()
{
	controller_->GetGameInstance()->GetSubsystem<UOnlineSessionSubsystem>()->StartGame();
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
