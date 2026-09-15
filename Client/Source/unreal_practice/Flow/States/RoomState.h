#pragma once

#include "ClientScreenState.h"
#include "RoomState.generated.h"

UCLASS()
class UNREAL_PRACTICE_API URoomState : public UClientScreenState
{
	GENERATED_BODY()

protected:
	virtual void OnEnter() override;
	virtual void OnExit() override;

private:
	UFUNCTION()
	void HandleRoomLeave();

	UFUNCTION()
	void HandleRoomStart();
};
