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

	// 로비 설정 메뉴와 같은 설정 창. 게임 중 ESC로 연다.
	UPROPERTY()
	TSubclassOf<USettingsWidget> settings_widget_class_;

	UPROPERTY(Transient)
	TObjectPtr<USettingsWidget> settings_widget_;

	bool was_mouse_cursor_visible_ = false;

	void ToggleMap();
	void CloseMap();
	void ToggleInventory();
	void CloseInventory();
	void ToggleSettings();
	void CloseSettings();
};
