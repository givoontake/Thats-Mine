#include "LoginWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

#define LOCTEXT_NAMESPACE "Login"

void ULoginWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	SetDesiredFocusWidget(id_input_);
	password_input_->SetIsPassword(true);
	login_button_->OnClicked.AddDynamic(this, &ULoginWidget::SubmitLogin);
	local_login_button_->OnClicked.AddDynamic(this, &ULoginWidget::SubmitLocalLogin);
	id_input_->OnTextCommitted.AddDynamic(this, &ULoginWidget::HandleIDCommitted);
	password_input_->OnTextCommitted.AddDynamic(this, &ULoginWidget::HandlePasswordCommitted);
}

void ULoginWidget::NativeDestruct()
{
	password_input_->SetText(FText::GetEmpty());
	Super::NativeDestruct();
}

void ULoginWidget::ShowStatus(const FText& message)
{
	status_text_->SetText(message);
}

void ULoginWidget::SubmitLogin()
{
	const FString user_id = id_input_->GetText().ToString().TrimStartAndEnd();
	if (user_id.IsEmpty())
	{
		ShowStatus(LOCTEXT("MissingID", "아이디를 입력해 주세요."));
		id_input_->SetKeyboardFocus();
		return;
	}
	const FString password = password_input_->GetText().ToString();
	if (password.IsEmpty())
	{
		ShowStatus(LOCTEXT("MissingPassword", "비밀번호를 입력해 주세요."));
		password_input_->SetKeyboardFocus();
		return;
	}
	ShowStatus(FText::GetEmpty());
	on_login_requested_.Broadcast(user_id, password);
	password_input_->SetText(FText::GetEmpty());
}

void ULoginWidget::HandleIDCommitted(const FText& text, ETextCommit::Type commit_type)
{
	if (commit_type == ETextCommit::OnEnter)
	{
		password_input_->SetKeyboardFocus();
	}
}

void ULoginWidget::HandlePasswordCommitted(const FText& text, ETextCommit::Type commit_type)
{
	if (commit_type == ETextCommit::OnEnter)
	{
		SubmitLogin();
	}
}

void ULoginWidget::SubmitLocalLogin()
{
	id_input_->SetText(FText::GetEmpty());
	password_input_->SetText(FText::GetEmpty());
	on_local_login_requested_.Broadcast();
}

#undef LOCTEXT_NAMESPACE
