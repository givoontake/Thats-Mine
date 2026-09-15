#include "ClientScreenState.h"

#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"

void UClientScreenState::Initialize(APlayerController* controller, TSoftClassPtr<UUserWidget> widget_class)
{
	controller_ = controller;
	widget_class_ = widget_class;
}

UUserWidget* UClientScreenState::Enter()
{
	if (screen_)
	{
		return screen_;
	}
	if (!controller_.IsValid())
	{
		return nullptr;
	}
	UClass* widget_class = widget_class_.LoadSynchronous();
	if (!widget_class)
	{
		UE_LOG(LogTemp, Error, TEXT("Unable to load screen widget %s."), *widget_class_.ToString());
		return nullptr;
	}
	screen_ = CreateWidget<UUserWidget>(controller_.Get(), widget_class);
	if (screen_)
	{
		OnEnter();
	}
	return screen_;
}

void UClientScreenState::Exit()
{
	if (!screen_)
	{
		return;
	}
	OnExit();
	screen_->RemoveFromParent();
	screen_ = nullptr;
}
