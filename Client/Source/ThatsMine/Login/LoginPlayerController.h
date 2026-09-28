#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Flow/client_state.h"
#include "LoginPlayerController.generated.h"

class ULoginWidget;
class ULobbyWidget;
class URoomWidget;
class UHammerCursorWidget;
class UClickDustWidget;
class UClientScreenState;

UCLASS()
class THATSMINE_API ALoginPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type end_play_reason) override;

	UPROPERTY(EditDefaultsOnly, Category = "Login")
	TSoftClassPtr<ULoginWidget> login_widget_class_ = TSoftClassPtr<ULoginWidget>(FSoftObjectPath(TEXT("/Game/Thats_Mine/UI/Login/WBP_Login.WBP_Login_C")));

	UPROPERTY(EditDefaultsOnly, Category = "Lobby")
	TSoftClassPtr<ULobbyWidget> lobby_widget_class_ = TSoftClassPtr<ULobbyWidget>(FSoftObjectPath(TEXT("/Game/Thats_Mine/UI/Lobby/WBP_Lobby.WBP_Lobby_C")));

	UPROPERTY(EditDefaultsOnly, Category = "Room")
	TSoftClassPtr<URoomWidget> room_widget_class_ = TSoftClassPtr<URoomWidget>(FSoftObjectPath(TEXT("/Game/Thats_Mine/UI/Room/WBP_Room.WBP_Room_C")));

private:
	UPROPERTY(Transient)
	TObjectPtr<UHammerCursorWidget> hammer_cursor_;

	UPROPERTY(Transient)
	TObjectPtr<UClickDustWidget> click_dust_;

	UFUNCTION()
	void HandleStateChanged(EClientState state);

	UPROPERTY(Transient)
	TMap<EClientState, TObjectPtr<UClientScreenState>> screen_states_;

	UPROPERTY(Transient)
	TObjectPtr<UClientScreenState> active_screen_state_;
};
