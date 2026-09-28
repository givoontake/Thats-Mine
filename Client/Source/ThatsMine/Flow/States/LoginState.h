#pragma once

#include "ClientScreenState.h"
#include "LoginState.generated.h"

UCLASS()
class THATSMINE_API ULoginState : public UClientScreenState
{
	GENERATED_BODY()

protected:
	virtual void OnEnter() override;
	virtual void OnExit() override;

private:
	void RefreshOnline();
	UFUNCTION()
	void HandleLoginRequested(const FString& user_id, const FString& password);

	UFUNCTION()
	void HandleLocalLoginRequested();
};
