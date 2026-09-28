#pragma once

#include "ClientScreenState.h"
#include "RoomState.generated.h"

UCLASS()
class THATSMINE_API URoomState : public UClientScreenState
{
	GENERATED_BODY()

protected:
	virtual void OnEnter() override;
	virtual void OnExit() override;

private:
	void RefreshOnline();
	UFUNCTION()
	void HandleReady(bool is_ready);
	UFUNCTION()
	void HandleKick(int64 player_key);
	UFUNCTION()
	void HandleRoomLeave();

	UFUNCTION()
	void HandleRoomStart();
};
