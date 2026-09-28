#pragma once

#include "ClientScreenState.h"
#include "LobbyState.generated.h"

UCLASS()
class THATSMINE_API ULobbyState : public UClientScreenState
{
	GENERATED_BODY()

protected:
	virtual void OnEnter() override;
	virtual void OnExit() override;

private:
	void RefreshOnline();
	UFUNCTION()
	void HandleQuickJoin();
	UFUNCTION()
	void HandleLobbyRefresh();

	UFUNCTION()
	void HandleLobbyCreate(const FString& room_name, bool is_private, const FString& password);

	UFUNCTION()
	void HandleLobbyJoin(int64 room_key, const FString& password);
};
