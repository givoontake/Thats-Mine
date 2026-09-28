#include "LobbyState.h"

#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Flow/ClientStateSubsystem.h"
#include "Local/LocalSessionSubsystem.h"
#include "UI/Lobby/LobbyWidget.h"
#include "Network/OnlineSessionSubsystem.h"

void ULobbyState::OnEnter()
{
	if (!controller_->GetGameInstance()->GetSubsystem<UClientStateSubsystem>()->IsLocalMode())
	{
		controller_->GetGameInstance()->GetSubsystem<UOnlineSessionSubsystem>()->on_changed_.AddUObject(this, &ULobbyState::RefreshOnline);
		CastChecked<ULobbyWidget>(screen_)->on_quick_join_requested_.AddDynamic(this, &ULobbyState::HandleQuickJoin);
	}
	ULobbyWidget* lobby_widget = CastChecked<ULobbyWidget>(screen_);
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
	if (!controller_->GetGameInstance()->GetSubsystem<UClientStateSubsystem>()->IsLocalMode())
	{
		controller_->GetGameInstance()->GetSubsystem<UOnlineSessionSubsystem>()->RefreshRooms();
		return;
	}
	ULobbyWidget* lobby_widget = CastChecked<ULobbyWidget>(screen_);
	lobby_widget->SetRooms(controller_->GetGameInstance()->GetSubsystem<ULocalSessionSubsystem>()->GetRooms());
	lobby_widget->ShowStatus(NSLOCTEXT("LocalSession", "LobbyNotice", "로컬 테스트 · 비공개 예제 방 비밀번호: 1234"));
}

void ULobbyState::HandleLobbyCreate(const FString& room_name, bool is_private, const FString& password)
{
	if (!controller_->GetGameInstance()->GetSubsystem<UClientStateSubsystem>()->IsLocalMode()) {
		controller_->GetGameInstance()->GetSubsystem<UOnlineSessionSubsystem>()->CreateRoom(room_name, is_private, password);
		return;
	}
	ULocalSessionSubsystem* local = controller_->GetGameInstance()->GetSubsystem<ULocalSessionSubsystem>();
	const FLocalRoomResult result = local->CreateRoom(room_name, is_private, password);
	if (!result.is_success)
	{
		CastChecked<ULobbyWidget>(screen_)->ShowStatus(result.message);
		return;
	}
	controller_->GetGameInstance()->GetSubsystem<UClientStateSubsystem>()->ChangeState(EClientState::ROOM);
}

void ULobbyState::HandleLobbyJoin(int64 room_key, const FString& password)
{
	if (!controller_->GetGameInstance()->GetSubsystem<UClientStateSubsystem>()->IsLocalMode()) {
		controller_->GetGameInstance()->GetSubsystem<UOnlineSessionSubsystem>()->JoinRoom(room_key, password);
		return;
	}
	ULocalSessionSubsystem* local = controller_->GetGameInstance()->GetSubsystem<ULocalSessionSubsystem>();
	const FLocalRoomResult result = local->JoinRoom(room_key, password);
	if (!result.is_success)
	{
		CastChecked<ULobbyWidget>(screen_)->ShowStatus(result.message);
		return;
	}
	controller_->GetGameInstance()->GetSubsystem<UClientStateSubsystem>()->ChangeState(EClientState::ROOM);
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
