// Copyright Epic Games, Inc. All Rights Reserved.


#include "ThatsMinePlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "InputMappingContext.h"
#include "ThatsMineCameraManager.h"
#include "Blueprint/UserWidget.h"
#include "ThatsMine.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "Components/InputComponent.h"
#include "UI/Map/WorldMapWidget.h"
#include "UI/Inventory/InventoryWidget.h"

AThatsMinePlayerController::AThatsMinePlayerController()
{
	// set the player camera manager class
	PlayerCameraManagerClass = AThatsMineCameraManager::StaticClass();
	map_widget_class_ = UWorldMapWidget::StaticClass();
}

void AThatsMinePlayerController::FlushPressedKeys()
{
	const UUserWidget* screen = map_widget_ && map_widget_->IsInViewport() ? static_cast<UUserWidget*>(map_widget_)
		: inventory_widget_ && inventory_widget_->IsInViewport() ? static_cast<UUserWidget*>(inventory_widget_) : nullptr;
	if (screen && FSlateApplication::IsInitialized() && FSlateApplication::Get().IsActive())
	{
		const UGameViewportClient* viewport = GetWorld()->GetGameViewport();
		const TSharedPtr<SWindow> window = viewport ? viewport->GetWindow() : nullptr;
		// 화면 내부로 포커스를 옮길 때는 이동 키를 유지하고, 게임 창을 벗어나면 기존대로 해제한다.
		if (window && window->IsActive() && (screen->HasUserFocus(this) || screen->HasUserFocusedDescendants(this)))
		{
			return;
		}
	}
	Super::FlushPressedKeys();
}

void AThatsMinePlayerController::BeginPlay()
{
	Super::BeginPlay();

	
	// only spawn touch controls on local player controllers
	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogThatsMine, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
}

void AThatsMinePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		InputComponent->BindKey(EKeys::M, IE_Pressed, this, &AThatsMinePlayerController::ToggleMap);
		InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &AThatsMinePlayerController::ToggleInventory);

		// Add Input Mapping Context
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
	
}

bool AThatsMinePlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

void AThatsMinePlayerController::EndPlay(const EEndPlayReason::Type end_play_reason)
{
	CloseMap();
	CloseInventory();
	map_widget_ = nullptr;
	inventory_widget_ = nullptr;
	Super::EndPlay(end_play_reason);
}

void AThatsMinePlayerController::ToggleMap()
{
	if (map_widget_ && map_widget_->IsInViewport())
	{
		CloseMap();
		return;
	}
	if (!IsLocalController() || !GetPawn())
	{
		return;
	}
	if (inventory_widget_ && inventory_widget_->IsInViewport())
	{
		return;
	}
	if (!map_widget_)
	{
		map_widget_ = CreateWidget<UWorldMapWidget>(this, map_widget_class_);
		if (!map_widget_)
		{
		return;
		}
		map_widget_->on_close_requested_.BindUObject(this, &AThatsMinePlayerController::CloseMap);
	}
	was_mouse_cursor_visible_ = bShowMouseCursor;
	SetIgnoreLookInput(true);
	map_widget_->AddToPlayerScreen(200);
	bShowMouseCursor = true;
	FInputModeGameAndUI input_mode;
	input_mode.SetWidgetToFocus(map_widget_->TakeWidget());
	input_mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	input_mode.SetHideCursorDuringCapture(false);
	SetInputMode(input_mode);
}

void AThatsMinePlayerController::CloseMap()
{
	if (!map_widget_ || !map_widget_->IsInViewport())
	{
		return;
	}
	map_widget_->RemoveFromParent();
	SetIgnoreLookInput(false);
	bShowMouseCursor = was_mouse_cursor_visible_;
	SetInputMode(FInputModeGameOnly());
}

void AThatsMinePlayerController::ToggleInventory()
{
	if (inventory_widget_ && inventory_widget_->IsInViewport())
	{
		CloseInventory();
		return;
	}
	if (!IsLocalController() || !GetPawn())
	{
		return;
	}
	if (map_widget_ && map_widget_->IsInViewport())
	{
		return;
	}
	if (!inventory_widget_)
	{
		inventory_widget_ = CreateWidget<UInventoryWidget>(this, UInventoryWidget::StaticClass());
		inventory_widget_->on_close_requested_.BindUObject(this, &AThatsMinePlayerController::CloseInventory);
	}
	was_mouse_cursor_visible_ = bShowMouseCursor;
	SetIgnoreLookInput(true);
	inventory_widget_->AddToPlayerScreen(200);
	bShowMouseCursor = true;
	FInputModeGameAndUI input_mode;
	input_mode.SetWidgetToFocus(inventory_widget_->TakeWidget());
	input_mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	input_mode.SetHideCursorDuringCapture(false);
	SetInputMode(input_mode);
}

void AThatsMinePlayerController::CloseInventory()
{
	if (!inventory_widget_ || !inventory_widget_->IsInViewport())
	{
		return;
	}
	inventory_widget_->RemoveFromParent();
	SetIgnoreLookInput(false);
	bShowMouseCursor = was_mouse_cursor_visible_;
	SetInputMode(FInputModeGameOnly());
}
