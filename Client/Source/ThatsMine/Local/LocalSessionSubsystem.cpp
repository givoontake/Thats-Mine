#include "LocalSessionSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UI/Local/LocalPlayWidget.h"
#include "UObject/UObjectGlobals.h"
#include "Flow/ClientStateSubsystem.h"

void ULocalSessionSubsystem::Initialize(FSubsystemCollectionBase& collection)
{
	Super::Initialize(collection);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ULocalSessionSubsystem::HandleMapLoaded);
	FWorldDelegates::OnWorldCleanup.AddUObject(this, &ULocalSessionSubsystem::HandleWorldCleanup);
}

void ULocalSessionSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
	FWorldDelegates::OnWorldCleanup.RemoveAll(this);
	ClearGameplayControls();
	Super::Deinitialize();
}

void ULocalSessionSubsystem::StartGame()
{
	UClientStateSubsystem* state = GetGameInstance()->GetSubsystem<UClientStateSubsystem>();
	if (!state->IsLocalMode())
	{
		return;
	}
	state->ChangeState(EClientState::PLAYING);
	UGameplayStatics::OpenLevel(this, TEXT("/Game/Thats_Mine/Levels/Demo_Landmass"));
}

void ULocalSessionSubsystem::QuitGame()
{
	UClientStateSubsystem* state = GetGameInstance()->GetSubsystem<UClientStateSubsystem>();
	if (!state->IsLocalMode() || state->GetState() != EClientState::PLAYING)
	{
		return;
	}
	APlayerController* controller = gameplay_controller_.Get();
	ClearGameplayControls();
	UKismetSystemLibrary::QuitGame(this, controller, EQuitPreference::Quit, false);
}

void ULocalSessionSubsystem::HandleMapLoaded(UWorld* world)
{
	if (!world || world->GetGameInstance() != GetGameInstance())
	{
		return;
	}
	UClientStateSubsystem* state = GetGameInstance()->GetSubsystem<UClientStateSubsystem>();
	if (!state->IsLocalMode() || state->GetState() != EClientState::PLAYING)
	{
		return;
	}
	ClearGameplayControls();
	APlayerController* controller = world->GetFirstPlayerController();
	if (!controller || !controller->IsLocalController())
	{
		return;
	}
	gameplay_controller_ = controller;
	gameplay_world_ = world;
	return_input_ = NewObject<UInputComponent>(controller);
	return_input_->Priority = 100;
	return_input_->RegisterComponent();
	return_input_->BindKey(EKeys::F10, IE_Pressed, this, &ULocalSessionSubsystem::QuitGame).bExecuteWhenPaused = true;
	controller->PushInputComponent(return_input_);
	local_play_widget_ = CreateWidget<ULocalPlayWidget>(controller);
	if (local_play_widget_)
	{
		local_play_widget_->AddToViewport(100);
	}
}

void ULocalSessionSubsystem::HandleWorldCleanup(UWorld* world, bool has_session_ended, bool should_cleanup_resources)
{
	if (world == gameplay_world_.Get())
	{
		ClearGameplayControls();
	}
}

void ULocalSessionSubsystem::ClearGameplayControls()
{
	if (local_play_widget_)
	{
		local_play_widget_->RemoveFromParent();
		local_play_widget_ = nullptr;
	}
	if (return_input_)
	{
		if (APlayerController* controller = gameplay_controller_.Get())
		{
			controller->PopInputComponent(return_input_);
		}
		return_input_->DestroyComponent();
		return_input_ = nullptr;
	}
	gameplay_controller_.Reset();
	gameplay_world_.Reset();
}
