#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "local_room.h"
#include "LocalSessionSubsystem.generated.h"

class APlayerController;
class UInputComponent;
class ULocalPlayWidget;

UCLASS()
class UNREAL_PRACTICE_API ULocalSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& collection) override;
	virtual void Deinitialize() override;
	void ResetSession();
	TArray<FLobbyRoomInfo> GetRooms() const;
	TOptional<FLobbyRoomInfo> GetCurrentRoom() const;
	FLocalRoomResult CreateRoom(const FString& room_name, bool is_private, const FString& password);
	FLocalRoomResult JoinRoom(int64 room_key, const FString& password);
	void LeaveRoom();
	void StartGame();
	void ReturnToLobby();

private:
	void HandleMapLoaded(UWorld* world);
	void HandleWorldCleanup(UWorld* world, bool has_session_ended, bool should_cleanup_resources);
	void ClearGameplayControls();

	TArray<FLocalRoom> rooms_;
	int64 next_room_key_ = 1;
	int64 current_room_key_ = 0;
	TWeakObjectPtr<APlayerController> gameplay_controller_;
	TWeakObjectPtr<UWorld> gameplay_world_;

	UPROPERTY(Transient)
	TObjectPtr<UInputComponent> return_input_;

	UPROPERTY(Transient)
	TObjectPtr<ULocalPlayWidget> local_play_widget_;
};
