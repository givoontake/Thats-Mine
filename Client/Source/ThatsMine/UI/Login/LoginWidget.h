#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/EditableTextBox.h"
#include "LoginWidget.generated.h"

class UButton;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLoginRequested, const FString&, user_id, const FString&, password);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLocalLoginRequested);

UCLASS(Abstract)
class THATSMINE_API ULoginWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void ShowStatus(const FText& message);
	void SetPending(bool is_pending);

	UPROPERTY(BlueprintAssignable, Category = "Login")
	FOnLoginRequested on_login_requested_;

	UPROPERTY(BlueprintAssignable, Category = "Login")
	FOnLocalLoginRequested on_local_login_requested_;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> id_input_;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> password_input_;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> login_button_;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> local_login_button_;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> status_text_;

private:
	UFUNCTION()
	void SubmitLogin();

	UFUNCTION()
	void SubmitLocalLogin();

	UFUNCTION()
	void HandleIDCommitted(const FText& text, ETextCommit::Type commit_type);

	UFUNCTION()
	void HandlePasswordCommitted(const FText& text, ETextCommit::Type commit_type);
};
