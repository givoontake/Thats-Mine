#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateTypes.h"
#include "UI/Lobby/lobby_room_info.h"
#include "room_member_info.h"
#include "RoomWidget.generated.h"

class UButton;
class UTextBlock;
class UUniformGridPanel;
class URoomMemberWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRoomLeaveRequested);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRoomStartRequested);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRoomReadyRequested, bool, is_ready);

UCLASS(Abstract)
class UNREAL_PRACTICE_API URoomWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Room")
	void SetRoom(const FLobbyRoomInfo& room, const TArray<FRoomMemberInfo>& members, int64 local_player_key, bool can_start);

	UPROPERTY(BlueprintAssignable, Category = "Room")
	FOnRoomLeaveRequested on_leave_requested_;
	UPROPERTY(BlueprintAssignable, Category = "Room")
	FOnRoomStartRequested on_start_requested_;
	UPROPERTY(BlueprintAssignable, Category = "Room")
	FOnRoomReadyRequested on_ready_requested_;

protected:
	virtual void NativeOnInitialized() override;

	UPROPERTY(EditDefaultsOnly, Category = "Appearance")
	FSlateBrush selected_button_brush_;

	UPROPERTY(EditDefaultsOnly, Category = "Room")
	TSoftClassPtr<URoomMemberWidget> member_widget_class_ = TSoftClassPtr<URoomMemberWidget>(FSoftObjectPath(TEXT("/Game/Thats_Mine/UI/Room/WBP_RoomMember.WBP_RoomMember_C")));

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UUniformGridPanel> members_grid_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> room_name_text_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> room_info_text_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> status_text_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> action_button_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> action_text_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> leave_button_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> leave_dialog_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> leave_confirm_button_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> leave_cancel_button_;

private:
	UFUNCTION()
	void HandleAction();
	UFUNCTION()
	void ShowLeaveDialog();
	UFUNCTION()
	void CancelLeave();
	UFUNCTION()
	void ConfirmLeave();

	UPROPERTY(Transient)
	TArray<TObjectPtr<URoomMemberWidget>> member_widgets_;
	FButtonStyle action_style_;
	bool is_host_ = false;
	bool is_ready_ = false;
};
