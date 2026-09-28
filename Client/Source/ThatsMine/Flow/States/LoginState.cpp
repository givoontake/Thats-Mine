#include "LoginState.h"

#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Flow/ClientStateSubsystem.h"
#include "UI/Login/LoginWidget.h"
#include "Network/OnlineSessionSubsystem.h"

void ULoginState::OnEnter()
{
	ULoginWidget* login_widget = CastChecked<ULoginWidget>(screen_);
	login_widget->on_login_requested_.AddDynamic(this, &ULoginState::HandleLoginRequested);
	login_widget->on_local_login_requested_.AddDynamic(this, &ULoginState::HandleLocalLoginRequested);
	controller_->GetGameInstance()->GetSubsystem<UOnlineSessionSubsystem>()->on_changed_.AddUObject(this, &ULoginState::RefreshOnline);
	RefreshOnline();
}

void ULoginState::OnExit()
{
	controller_->GetGameInstance()->GetSubsystem<UOnlineSessionSubsystem>()->on_changed_.RemoveAll(this);
	ULoginWidget* login_widget = CastChecked<ULoginWidget>(screen_);
	login_widget->on_login_requested_.RemoveDynamic(this, &ULoginState::HandleLoginRequested);
	login_widget->on_local_login_requested_.RemoveDynamic(this, &ULoginState::HandleLocalLoginRequested);
}

void ULoginState::HandleLoginRequested(const FString& user_id, const FString& password)
{
	controller_->GetGameInstance()->GetSubsystem<UOnlineSessionSubsystem>()->Login(user_id, password);
}

void ULoginState::HandleLocalLoginRequested()
{
	controller_->GetGameInstance()->GetSubsystem<UClientStateSubsystem>()->StartLocalSession();
}

void ULoginState::RefreshOnline()
{
	if (!screen_) return;
	const auto* online = controller_->GetGameInstance()->GetSubsystem<UOnlineSessionSubsystem>();
	auto* widget = CastChecked<ULoginWidget>(screen_);
	widget->ShowStatus(online->GetStatus());
	widget->SetPending(online->IsPending());
}
