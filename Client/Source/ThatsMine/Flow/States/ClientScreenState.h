#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ClientScreenState.generated.h"

class APlayerController;
class UUserWidget;

UCLASS(Abstract)
class THATSMINE_API UClientScreenState : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(APlayerController* controller, TSoftClassPtr<UUserWidget> widget_class);
	UUserWidget* Enter();
	void Exit();

protected:
	virtual void OnEnter() PURE_VIRTUAL(UClientScreenState::OnEnter, );
	virtual void OnExit() PURE_VIRTUAL(UClientScreenState::OnExit, );

	UPROPERTY(Transient)
	TWeakObjectPtr<APlayerController> controller_;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> screen_;

private:
	UPROPERTY(Transient)
	TSoftClassPtr<UUserWidget> widget_class_;
};
