// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ThatsMinePlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;
class UWorldMapWidget;
class UInventoryWidget;
class USettingsWidget;
class UPauseMenuWidget;
class UHelpWidget;
class UScaleBox;

/**
 *  Simple first person Player Controller
 *  Manages the input mapping context.
 *  Overrides the Player Camera Manager class.
 */
UCLASS(abstract, config="Game")
class THATSMINE_API AThatsMinePlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:

	/** Constructor */
	AThatsMinePlayerController();
	virtual void FlushPressedKeys() override;

protected:

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Gameplay initialization */
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type end_play_reason) override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

	UPROPERTY(EditDefaultsOnly, Category = "Map")
	TSubclassOf<UWorldMapWidget> map_widget_class_;

private:
	UPROPERTY(Transient)
	TObjectPtr<UWorldMapWidget> map_widget_;

	UPROPERTY(Transient)
	TObjectPtr<UInventoryWidget> inventory_widget_;

	// 게임 중 ESC로 여는 메뉴
	UPROPERTY(Transient)
	TObjectPtr<UPauseMenuWidget> pause_menu_widget_;

	// 게임 중 도움말 키로 여는 도움말
	UPROPERTY(Transient)
	TObjectPtr<UHelpWidget> help_widget_;

	// 이동 키를 키 설정으로 바꾼 입력 매핑. 키 설정이 바뀌면 새로 만들어 바꿔 넣는다.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputMappingContext>> applied_mapping_contexts_;
	FDelegateHandle key_bindings_changed_handle_;

	// 로비 설정 메뉴와 같은 설정 창. 게임 중 ESC 메뉴에서 연다.
	UPROPERTY()
	TSubclassOf<USettingsWidget> settings_widget_class_;

	UPROPERTY(Transient)
	TObjectPtr<USettingsWidget> settings_widget_;

	// 설정 창을 감싸 화면 크기에 맞춰 키우는 틀. 설정 창 크기를 정하는 크기 상자를 담는다.
	UPROPERTY(Transient)
	TObjectPtr<UScaleBox> settings_frame_;

	bool was_mouse_cursor_visible_ = false;

	// 키 설정의 키로 창 열기 키를 묶고 이동 입력 매핑을 바꿔 넣는다. 캐릭터의 키 입력도 다시 묶는다.
	void ApplyKeyBindings();
	void ApplyMappingContexts();
	void ToggleHelp();
	void CloseHelp();
	void ToggleMap();
	void CloseMap();
	void ToggleInventory();
	void CloseInventory();
	// ESC 메뉴를 열거나 닫는다. 지도·인벤토리·설정 창이 열려 있으면 그 창이 ESC를 받아 닫는다.
	void TogglePauseMenu();
	void ClosePauseMenu();
	void ToggleSettings();
	void CloseSettings();
	// 게임에서 나가 로비로 돌아간다.
	void LeaveToLobby();
	void QuitGame();
};
