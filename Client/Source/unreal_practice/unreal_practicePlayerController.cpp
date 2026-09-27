// Copyright Epic Games, Inc. All Rights Reserved.


#include "unreal_practicePlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "unreal_practiceCameraManager.h"
#include "Blueprint/UserWidget.h"
#include "unreal_practice.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "Components/InputComponent.h"
#include "UI/Map/WorldMapWidget.h"
#include "UI/Inventory/InventoryWidget.h"

Aunreal_practicePlayerController::Aunreal_practicePlayerController()
{
	// set the player camera manager class
	PlayerCameraManagerClass = Aunreal_practiceCameraManager::StaticClass();
	map_widget_class_ = UWorldMapWidget::StaticClass();
}

void Aunreal_practicePlayerController::BeginPlay()
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

			UE_LOG(Logunreal_practice, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
}

void Aunreal_practicePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		InputComponent->BindKey(EKeys::M, IE_Pressed, this, &Aunreal_practicePlayerController::ToggleMap);
		InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &Aunreal_practicePlayerController::ToggleInventory);

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

bool Aunreal_practicePlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

void Aunreal_practicePlayerController::EndPlay(const EEndPlayReason::Type end_play_reason)
{
	CloseMap();
	CloseInventory();
	map_widget_ = nullptr;
	inventory_widget_ = nullptr;
	Super::EndPlay(end_play_reason);
}

void Aunreal_practicePlayerController::ToggleMap()
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
	CloseInventory();
	if (!map_widget_)
	{
		map_widget_ = CreateWidget<UWorldMapWidget>(this, map_widget_class_);
		if (!map_widget_)
		{
		return;
		}
		map_widget_->on_close_requested_.BindUObject(this, &Aunreal_practicePlayerController::CloseMap);
		map_widget_->on_inventory_requested_.BindUObject(this, &Aunreal_practicePlayerController::ToggleInventory);
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

void Aunreal_practicePlayerController::CloseMap()
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

void Aunreal_practicePlayerController::ToggleInventory()
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
	CloseMap();
	if (!inventory_widget_)
	{
		inventory_widget_ = CreateWidget<UInventoryWidget>(this, UInventoryWidget::StaticClass());
		inventory_widget_->on_close_requested_.BindUObject(this, &Aunreal_practicePlayerController::CloseInventory);
		inventory_widget_->on_map_requested_.BindUObject(this, &Aunreal_practicePlayerController::ToggleMap);
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

void Aunreal_practicePlayerController::CloseInventory()
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
