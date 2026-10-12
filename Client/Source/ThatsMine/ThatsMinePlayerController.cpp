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
#include "UI/Settings/SettingsWidget.h"
#include "Blueprint/GameViewportSubsystem.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "UObject/ConstructorHelpers.h"

AThatsMinePlayerController::AThatsMinePlayerController()
{
	// set the player camera manager class
	PlayerCameraManagerClass = AThatsMineCameraManager::StaticClass();
	map_widget_class_ = UWorldMapWidget::StaticClass();
	static ConstructorHelpers::FClassFinder<USettingsWidget> settings_widget_class(TEXT("/Game/Thats_Mine/UI/Settings/WBP_Settings"));
	settings_widget_class_ = settings_widget_class.Class;
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
		InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AThatsMinePlayerController::ToggleSettings);

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
	CloseSettings();
	map_widget_ = nullptr;
	inventory_widget_ = nullptr;
	settings_widget_ = nullptr;
	settings_frame_ = nullptr;
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
	if ((inventory_widget_ && inventory_widget_->IsInViewport()) || (settings_frame_ && settings_frame_->IsInViewport()))
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
	if ((map_widget_ && map_widget_->IsInViewport()) || (settings_frame_ && settings_frame_->IsInViewport()))
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

// 로비 설정 메뉴와 같은 크기(1600x900 화면 기준 885x630)로 화면 가운데에 띄운다. 지도·인벤토리가 열려 있으면 그쪽이 ESC를 받아 닫는다.
void AThatsMinePlayerController::ToggleSettings()
{
	if (settings_frame_ && settings_frame_->IsInViewport())
	{
		CloseSettings();
		return;
	}
	if (!IsLocalController() || !GetPawn() || !settings_widget_class_)
	{
		return;
	}
	if ((map_widget_ && map_widget_->IsInViewport()) || (inventory_widget_ && inventory_widget_->IsInViewport()))
	{
		return;
	}
	if (!settings_widget_)
	{
		settings_widget_ = CreateWidget<USettingsWidget>(this, settings_widget_class_);
		settings_widget_->on_close_requested_.BindUObject(this, &AThatsMinePlayerController::CloseSettings);
		// 로비처럼 배치 단계에서 키워 글자를 키운 크기로 다시 그린다. 그림 배율로 키우면 글자가 흐려진다.
		USizeBox* size_box = NewObject<USizeBox>(this);
		size_box->SetWidthOverride(885.0f);
		size_box->SetHeightOverride(630.0f);
		settings_frame_ = NewObject<UScaleBox>(this);
		settings_frame_->SetStretch(EStretch::UserSpecified);
		settings_frame_->AddChild(size_box);
	}
	const FVector2D viewport_size = UWidgetLayoutLibrary::GetViewportSize(this);
	const float scale = FMath::Min(viewport_size.X / 1600.0f, viewport_size.Y / 900.0f) / UWidgetLayoutLibrary::GetViewportScale(this);
	was_mouse_cursor_visible_ = bShowMouseCursor;
	SetIgnoreLookInput(true);
	// 열 때마다 설정 창을 틀에 다시 넣어 저장된 값으로 새로 맞추게 한다.
	CastChecked<USizeBox>(settings_frame_->GetContent())->AddChild(settings_widget_);
	settings_frame_->SetUserSpecifiedScale(scale);
	FGameViewportWidgetSlot slot;
	slot.Anchors = FAnchors(0.5f, 0.5f);
	slot.Alignment = FVector2D(0.5, 0.5);
	slot.Offsets = FMargin(0.0f, 0.0f, 885.0f * scale, 630.0f * scale);
	slot.ZOrder = 200;
	UGameViewportSubsystem::Get()->AddWidgetForPlayer(settings_frame_, GetLocalPlayer(), slot);
	bShowMouseCursor = true;
	FInputModeGameAndUI input_mode;
	input_mode.SetWidgetToFocus(settings_widget_->TakeWidget());
	input_mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	input_mode.SetHideCursorDuringCapture(false);
	SetInputMode(input_mode);
}

void AThatsMinePlayerController::CloseSettings()
{
	if (!settings_frame_ || !settings_frame_->IsInViewport())
	{
		return;
	}
	settings_widget_->RemoveFromParent();
	settings_frame_->RemoveFromParent();
	SetIgnoreLookInput(false);
	bShowMouseCursor = was_mouse_cursor_visible_;
	SetInputMode(FInputModeGameOnly());
}
