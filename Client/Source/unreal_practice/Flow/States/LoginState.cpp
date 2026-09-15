#include "LoginState.h"

#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "Flow/ClientStateSubsystem.h"
#include "UI/Login/LoginWidget.h"

void ULoginState::OnEnter()
{
	ULoginWidget* login_widget = CastChecked<ULoginWidget>(screen_);
	login_widget->on_login_requested_.AddDynamic(this, &ULoginState::HandleLoginRequested);
	login_widget->on_local_login_requested_.AddDynamic(this, &ULoginState::HandleLocalLoginRequested);
}

void ULoginState::OnExit()
{
	ULoginWidget* login_widget = CastChecked<ULoginWidget>(screen_);
	login_widget->on_login_requested_.RemoveDynamic(this, &ULoginState::HandleLoginRequested);
	login_widget->on_local_login_requested_.RemoveDynamic(this, &ULoginState::HandleLocalLoginRequested);
}

void ULoginState::HandleLoginRequested(const FString& user_id, const FString& password)
{
	// 인증 패킷이 추가되면 이 요청을 인증 처리에 연결한다.
	CastChecked<ULoginWidget>(screen_)->ShowStatus(NSLOCTEXT("Login", "Unavailable", "로그인 서비스를 준비 중입니다."));
}

void ULoginState::HandleLocalLoginRequested()
{
	controller_->GetGameInstance()->GetSubsystem<UClientStateSubsystem>()->StartLocalSession();
}
