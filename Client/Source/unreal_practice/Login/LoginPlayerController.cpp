#include "LoginPlayerController.h"

#include "Engine/GameInstance.h"
#include "Flow/ClientStateSubsystem.h"
#include "Flow/States/ClientScreenState.h"
#include "Flow/States/LoginState.h"
#include "Flow/States/LobbyState.h"
#include "Flow/States/RoomState.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "UI/Cursor/HammerCursorWidget.h"
#include "UI/Effects/ClickDustWidget.h"

void ALoginPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController())
	{
		return;
	}
	screen_states_.Add(EClientState::LOGIN, NewObject<ULoginState>(this));
	screen_states_.Add(EClientState::LOBBY, NewObject<ULobbyState>(this));
	screen_states_.Add(EClientState::ROOM, NewObject<URoomState>(this));
	screen_states_[EClientState::LOGIN]->Initialize(this, TSoftClassPtr<UUserWidget>(login_widget_class_.ToSoftObjectPath()));
	screen_states_[EClientState::LOBBY]->Initialize(this, TSoftClassPtr<UUserWidget>(lobby_widget_class_.ToSoftObjectPath()));
	screen_states_[EClientState::ROOM]->Initialize(this, TSoftClassPtr<UUserWidget>(room_widget_class_.ToSoftObjectPath()));
	hammer_cursor_ = CreateWidget<UHammerCursorWidget>(this);
	if (hammer_cursor_)
	{
		SetMouseCursorWidget(EMouseCursor::Default, hammer_cursor_);
		SetMouseCursorWidget(EMouseCursor::Hand, hammer_cursor_);
	}
	click_dust_ = CreateWidget<UClickDustWidget>(this);
	if (click_dust_)
	{
		click_dust_->AddToViewport(1000);
	}
	UClientStateSubsystem* state = GetGameInstance()->GetSubsystem<UClientStateSubsystem>();
	state->on_state_changed_.AddDynamic(this, &ALoginPlayerController::HandleStateChanged);
	HandleStateChanged(state->GetState());
}

void ALoginPlayerController::EndPlay(const EEndPlayReason::Type end_play_reason)
{
	if (UGameInstance* game_instance = GetGameInstance())
	{
		if (UClientStateSubsystem* state = game_instance->GetSubsystem<UClientStateSubsystem>())
		{
			state->on_state_changed_.RemoveDynamic(this, &ALoginPlayerController::HandleStateChanged);
		}
	}
	if (active_screen_state_)
	{
		active_screen_state_->Exit();
		active_screen_state_ = nullptr;
	}
	screen_states_.Reset();
	if (click_dust_)
	{
		click_dust_->ResetEffects();
		click_dust_->RemoveFromParent();
		click_dust_ = nullptr;
	}
	if (hammer_cursor_)
	{
		SetMouseCursorWidget(EMouseCursor::Default, nullptr);
		SetMouseCursorWidget(EMouseCursor::Hand, nullptr);
		hammer_cursor_ = nullptr;
	}
	Super::EndPlay(end_play_reason);
}

void ALoginPlayerController::HandleStateChanged(EClientState state)
{
	UClientScreenState* next_state = screen_states_.FindRef(state);
	if (active_screen_state_ && active_screen_state_ == next_state)
	{
		return;
	}
	if (active_screen_state_)
	{
		active_screen_state_->Exit();
		active_screen_state_ = nullptr;
	}
	if (!next_state)
	{
		if (click_dust_)
		{
			click_dust_->ResetEffects();
		}
		SetInputMode(FInputModeGameOnly());
		bShowMouseCursor = false;
		return;
	}
	UUserWidget* screen = next_state->Enter();
	if (!screen)
	{
		UE_LOG(LogTemp, Error, TEXT("Unable to create menu widget for state %d."), static_cast<int32>(state));
		return;
	}
	active_screen_state_ = next_state;
	screen->AddToViewport();
	if (click_dust_ && screen->WidgetTree)
	{
		screen->WidgetTree->ForEachWidgetAndDescendants([this](UWidget* widget)
		{
			if (UButton* button = Cast<UButton>(widget))
			{
				button->OnClicked.AddUniqueDynamic(click_dust_, &UClickDustWidget::PlayAtCursor);
			}
		});
	}
	FInputModeUIOnly input_mode;
	input_mode.SetWidgetToFocus(screen->TakeWidget());
	input_mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(input_mode);
	bShowMouseCursor = true;
}
