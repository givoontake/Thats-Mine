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
#include "Components/InputComponent.h"
#include "UI/Map/WorldMapWidget.h"
#include "UI/Inventory/InventoryWidget.h"
#include "UI/Settings/SettingsWidget.h"
#include "UI/Menu/PauseMenuWidget.h"
#include "UI/Help/HelpWidget.h"
#include "Settings/KeyBindingSubsystem.h"
#include "Player/PlayerCharacter.h"
#include "Flow/ClientStateSubsystem.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Network/OnlineSessionSubsystem.h"
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
		: inventory_widget_ && inventory_widget_->IsInViewport() ? static_cast<UUserWidget*>(inventory_widget_)
		: help_widget_ && help_widget_->IsInViewport() ? static_cast<UUserWidget*>(help_widget_) : nullptr;
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

void AThatsMinePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		ApplyKeyBindings();
		if (!key_bindings_changed_handle_.IsValid())
		{
			key_bindings_changed_handle_ = GetGameInstance()->GetSubsystem<UKeyBindingSubsystem>()->on_keys_changed_.AddUObject(this, &AThatsMinePlayerController::ApplyKeyBindings);
		}
	}

}

void AThatsMinePlayerController::ApplyKeyBindings()
{
	const UKeyBindingSubsystem* bindings = GetGameInstance()->GetSubsystem<UKeyBindingSubsystem>();
	InputComponent->KeyBindings.Reset();
	InputComponent->BindKey(bindings->GetKey(EKeyAction::MAP), IE_Pressed, this, &AThatsMinePlayerController::ToggleMap);
	InputComponent->BindKey(bindings->GetKey(EKeyAction::INVENTORY), IE_Pressed, this, &AThatsMinePlayerController::ToggleInventory);
	InputComponent->BindKey(bindings->GetKey(EKeyAction::HELP), IE_Pressed, this, &AThatsMinePlayerController::ToggleHelp);
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AThatsMinePlayerController::TogglePauseMenu);
	ApplyMappingContexts();
	if (APlayerCharacter* character = Cast<APlayerCharacter>(GetPawn()))
	{
		character->RebindKeys();
	}
}

// 입력 매핑의 기본 이동 키(W·S·A·D)를 키 설정의 이동 키로 바꾼 사본을 만들어 넣는다. 원본 입력 매핑 에셋은 바꾸지 않는다.
void AThatsMinePlayerController::ApplyMappingContexts()
{
	UEnhancedInputLocalPlayerSubsystem* input = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (!input)
	{
		return;
	}
	for (UInputMappingContext* context : applied_mapping_contexts_)
	{
		input->RemoveMappingContext(context);
	}
	applied_mapping_contexts_.Reset();
	const UKeyBindingSubsystem* bindings = GetGameInstance()->GetSubsystem<UKeyBindingSubsystem>();
	const EKeyAction move_actions[] = { EKeyAction::MOVE_FORWARD, EKeyAction::MOVE_BACKWARD, EKeyAction::MOVE_LEFT, EKeyAction::MOVE_RIGHT };
	TArray<UInputMappingContext*> sources(DefaultMappingContexts);
	sources.Append(MobileExcludedMappingContexts);
	for (UInputMappingContext* source : sources)
	{
		if (!source)
		{
			continue;
		}
		UInputMappingContext* context = DuplicateObject<UInputMappingContext>(source, this);
		for (int32 index = 0; index < context->GetMappings().Num(); ++index)
		{
			FEnhancedActionKeyMapping& mapping = context->GetMapping(index);
			for (const EKeyAction action : move_actions)
			{
				if (mapping.Key == UKeyBindingSubsystem::GetDefaultKey(action))
				{
					mapping.Key = bindings->GetKey(action);
					break;
				}
			}
		}
		input->AddMappingContext(context, 0);
		applied_mapping_contexts_.Add(context);
	}
}

void AThatsMinePlayerController::ToggleHelp()
{
	if (help_widget_ && help_widget_->IsInViewport())
	{
		CloseHelp();
		return;
	}
	if (!IsLocalController() || !GetPawn())
	{
		return;
	}
	if ((map_widget_ && map_widget_->IsInViewport()) || (inventory_widget_ && inventory_widget_->IsInViewport()) || (pause_menu_widget_ && pause_menu_widget_->IsInViewport()) || (settings_frame_ && settings_frame_->IsInViewport()))
	{
		return;
	}
	if (!help_widget_)
	{
		help_widget_ = CreateWidget<UHelpWidget>(this, UHelpWidget::StaticClass());
		help_widget_->on_close_requested_.BindUObject(this, &AThatsMinePlayerController::CloseHelp);
	}
	was_mouse_cursor_visible_ = bShowMouseCursor;
	SetIgnoreLookInput(true);
	help_widget_->AddToPlayerScreen(200);
	bShowMouseCursor = true;
	FInputModeGameAndUI input_mode;
	input_mode.SetWidgetToFocus(help_widget_->TakeWidget());
	input_mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	input_mode.SetHideCursorDuringCapture(false);
	SetInputMode(input_mode);
	if (APlayerCharacter* character = Cast<APlayerCharacter>(GetPawn()))
	{
		character->UpdateEquipmentDisplay();
	}
}

void AThatsMinePlayerController::CloseHelp()
{
	if (!help_widget_ || !help_widget_->IsInViewport())
	{
		return;
	}
	help_widget_->RemoveFromParent();
	SetIgnoreLookInput(false);
	bShowMouseCursor = was_mouse_cursor_visible_;
	SetInputMode(FInputModeGameOnly());
	if (APlayerCharacter* character = Cast<APlayerCharacter>(GetPawn()))
	{
		character->UpdateEquipmentDisplay();
	}
}

void AThatsMinePlayerController::EndPlay(const EEndPlayReason::Type end_play_reason)
{
	CloseMap();
	CloseInventory();
	CloseHelp();
	ClosePauseMenu();
	CloseSettings();
	if (key_bindings_changed_handle_.IsValid())
	{
		GetGameInstance()->GetSubsystem<UKeyBindingSubsystem>()->on_keys_changed_.Remove(key_bindings_changed_handle_);
		key_bindings_changed_handle_.Reset();
	}
	map_widget_ = nullptr;
	inventory_widget_ = nullptr;
	help_widget_ = nullptr;
	pause_menu_widget_ = nullptr;
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
	if (APlayerCharacter* character = Cast<APlayerCharacter>(GetPawn()))
	{
		character->UpdateEquipmentDisplay();
	}
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
	if (APlayerCharacter* character = Cast<APlayerCharacter>(GetPawn()))
	{
		character->UpdateEquipmentDisplay();
	}
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
	if (APlayerCharacter* character = Cast<APlayerCharacter>(GetPawn()))
	{
		character->UpdateEquipmentDisplay();
	}
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
	if (APlayerCharacter* character = Cast<APlayerCharacter>(GetPawn()))
	{
		character->UpdateEquipmentDisplay();
	}
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
		// 설정 창에서 ESC를 누르면 ESC 메뉴로 돌아간다.
		settings_widget_->on_close_requested_.BindWeakLambda(this, [this]()
		{
			CloseSettings();
			TogglePauseMenu();
		});
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
	if (APlayerCharacter* character = Cast<APlayerCharacter>(GetPawn()))
	{
		character->UpdateEquipmentDisplay();
	}
}

bool AThatsMinePlayerController::IsMenuOpen() const
{
	return (pause_menu_widget_ && pause_menu_widget_->IsInViewport()) || (settings_frame_ && settings_frame_->IsInViewport())
		|| (map_widget_ && map_widget_->IsInViewport()) || (inventory_widget_ && inventory_widget_->IsInViewport()) || (help_widget_ && help_widget_->IsInViewport());
}

// 메뉴를 열고 닫을 때마다 내 캐릭터의 장비 표시를 다시 그려 에임을 숨기거나 보인다.
void AThatsMinePlayerController::TogglePauseMenu()
{
	if (pause_menu_widget_ && pause_menu_widget_->IsInViewport())
	{
		ClosePauseMenu();
		return;
	}
	if (!IsLocalController() || !GetPawn())
	{
		return;
	}
	if ((map_widget_ && map_widget_->IsInViewport()) || (inventory_widget_ && inventory_widget_->IsInViewport()) || (help_widget_ && help_widget_->IsInViewport()) || (settings_frame_ && settings_frame_->IsInViewport()))
	{
		return;
	}
	if (!pause_menu_widget_)
	{
		pause_menu_widget_ = CreateWidget<UPauseMenuWidget>(this, UPauseMenuWidget::StaticClass());
		pause_menu_widget_->on_resume_requested_.BindUObject(this, &AThatsMinePlayerController::ClosePauseMenu);
		pause_menu_widget_->on_settings_requested_.BindWeakLambda(this, [this]()
		{
			ClosePauseMenu();
			ToggleSettings();
		});
		pause_menu_widget_->on_leave_requested_.BindUObject(this, &AThatsMinePlayerController::LeaveToLobby);
		pause_menu_widget_->on_quit_requested_.BindUObject(this, &AThatsMinePlayerController::QuitGame);
	}
	pause_menu_widget_->SetCanLeaveToLobby(!GetGameInstance()->GetSubsystem<UClientStateSubsystem>()->IsLocalMode());
	was_mouse_cursor_visible_ = bShowMouseCursor;
	SetIgnoreLookInput(true);
	pause_menu_widget_->AddToPlayerScreen(200);
	bShowMouseCursor = true;
	FInputModeGameAndUI input_mode;
	input_mode.SetWidgetToFocus(pause_menu_widget_->TakeWidget());
	input_mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	input_mode.SetHideCursorDuringCapture(false);
	SetInputMode(input_mode);
	if (APlayerCharacter* character = Cast<APlayerCharacter>(GetPawn()))
	{
		character->UpdateEquipmentDisplay();
	}
}

void AThatsMinePlayerController::ClosePauseMenu()
{
	if (!pause_menu_widget_ || !pause_menu_widget_->IsInViewport())
	{
		return;
	}
	pause_menu_widget_->RemoveFromParent();
	SetIgnoreLookInput(false);
	bShowMouseCursor = was_mouse_cursor_visible_;
	SetInputMode(FInputModeGameOnly());
	if (APlayerCharacter* character = Cast<APlayerCharacter>(GetPawn()))
	{
		character->UpdateEquipmentDisplay();
	}
}

// 서버가 방에서 내보내면 로비 레벨로 옮겨 간다. 그동안 메뉴는 닫아 둔다.
void AThatsMinePlayerController::LeaveToLobby()
{
	ClosePauseMenu();
	GetGameInstance()->GetSubsystem<UOnlineSessionSubsystem>()->LeaveRoom();
}

void AThatsMinePlayerController::QuitGame()
{
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
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
	if (APlayerCharacter* character = Cast<APlayerCharacter>(GetPawn()))
	{
		character->UpdateEquipmentDisplay();
	}
}
