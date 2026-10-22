#include "LobbyState.h"

#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "UI/Lobby/LobbyWidget.h"
#include "Network/OnlineSessionSubsystem.h"

void ULobbyState::OnEnter()
{
	controller_->GetGameInstance()->GetSubsystem<UOnlineSessionSubsystem>()->on_changed_.AddUObject(this, &ULobbyState::RefreshOnline);
	ULobbyWidget* lobby_widget = CastChecked<ULobbyWidget>(screen_);
	lobby_widget->on_quick_join_requested_.AddDynamic(this, &ULobbyState::HandleQuickJoin);
	lobby_widget->on_refresh_requested_.AddDynamic(this, &ULobbyState::HandleLobbyRefresh);
	lobby_widget->on_create_requested_.AddDynamic(this, &ULobbyState::HandleLobbyCreate);
	lobby_widget->on_join_requested_.AddDynamic(this, &ULobbyState::HandleLobbyJoin);
	HandleLobbyRefresh();
}

void ULobbyState::OnExit()
{
	controller_->GetGameInstance()->GetSubsystem<UOnlineSessionSubsystem>()->on_changed_.RemoveAll(this);
	CastChecked<ULobbyWidget>(screen_)->on_quick_join_requested_.RemoveAll(this);
	ULobbyWidget* lobby_widget = CastChecked<ULobbyWidget>(screen_);
	lobby_widget->on_refresh_requested_.RemoveAll(this);
	lobby_widget->on_create_requested_.RemoveAll(this);
	lobby_widget->on_join_requested_.RemoveAll(this);
}

void ULobbyState::HandleLobbyRefresh()
{
	if (!screen_) return;
	controller_->GetGameInstance()->GetSubsystem<UOnlineSessionSubsystem>()->RefreshRooms();
}

void ULobbyState::HandleLobbyCreate(const FString& room_name, bool is_private, const FString& password)
{
	controller_->GetGameInstance()->GetSubsystem<UOnlineSessionSubsystem>()->CreateRoom(room_name, is_private, password);
}

void ULobbyState::HandleLobbyJoin(int64 room_key, const FString& password)
{
	controller_->GetGameInstance()->GetSubsystem<UOnlineSessionSubsystem>()->JoinRoom(room_key, password);
}

void ULobbyState::HandleQuickJoin()
{
	controller_->GetGameInstance()->GetSubsystem<UOnlineSessionSubsystem>()->QuickJoin();
}

void ULobbyState::RefreshOnline()
{
	if (!screen_) return;
	const auto* online = controller_->GetGameInstance()->GetSubsystem<UOnlineSessionSubsystem>();
	auto* widget = CastChecked<ULobbyWidget>(screen_);
	widget->SetRooms(online->GetRooms());
	widget->ShowStatus(online->GetStatus());
	widget->SetPending(online->IsPending());
}
