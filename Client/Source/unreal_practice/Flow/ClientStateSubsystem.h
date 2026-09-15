#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "client_state.h"
#include "ClientStateSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnClientStateChanged, EClientState, state);

UCLASS()
class UNREAL_PRACTICE_API UClientStateSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Client State")
	EClientState GetState() const { return state_; }

	UFUNCTION(BlueprintCallable, Category = "Client State")
	void ChangeState(EClientState state);

	UFUNCTION(BlueprintCallable, Category = "Client State")
	void StartLocalSession();

	UFUNCTION(BlueprintPure, Category = "Client State")
	bool IsLocalMode() const { return is_local_mode_; }

	UPROPERTY(BlueprintAssignable, Category = "Client State")
	FOnClientStateChanged on_state_changed_;

private:
	UPROPERTY(Transient)
	EClientState state_ = EClientState::LOGIN;

	UPROPERTY(Transient)
	bool is_local_mode_ = false;
};
