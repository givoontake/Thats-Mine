#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LocalSessionSubsystem.generated.h"

class APlayerController;
class UInputComponent;
class ULocalPlayWidget;

UCLASS()
class THATSMINE_API ULocalSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& collection) override;
	virtual void Deinitialize() override;
	void StartGame();
	void QuitGame();

private:
	void HandleMapLoaded(UWorld* world);
	void HandleWorldCleanup(UWorld* world, bool has_session_ended, bool should_cleanup_resources);
	void ClearGameplayControls();

	TWeakObjectPtr<APlayerController> gameplay_controller_;
	TWeakObjectPtr<UWorld> gameplay_world_;

	UPROPERTY(Transient)
	TObjectPtr<UInputComponent> return_input_;

	UPROPERTY(Transient)
	TObjectPtr<ULocalPlayWidget> local_play_widget_;
};
