#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateTypes.h"
#include "lobby_room_info.h"
#include "Network/network_connection_state.h"
#include "LobbyWidget.generated.h"

class UButton;
class UCheckBox;
class UEditableTextBox;
class UListView;
class UTextBlock;
class UWidgetSwitcher;
class USettingsWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLobbyRefreshRequested);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnLobbyCreateRequested, const FString&, room_name, bool, is_private, const FString&, password);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLobbyJoinRequested, int64, room_key, const FString&, password);

UCLASS(Abstract)
class THATSMINE_API ULobbyWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void SetRooms(const TArray<FLobbyRoomInfo>& rooms);

	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void ShowStatus(const FText& message);
	void SetPending(bool is_pending);
	void SetMatching(bool is_matching, bool is_canceling);
	FOnLobbyRefreshRequested on_quick_join_requested_;
	FOnLobbyRefreshRequested on_cancel_matching_requested_;

	UPROPERTY(BlueprintAssignable, Category = "Lobby")
	FOnLobbyRefreshRequested on_refresh_requested_;

	UPROPERTY(BlueprintAssignable, Category = "Lobby")
	FOnLobbyCreateRequested on_create_requested_;

	UPROPERTY(BlueprintAssignable, Category = "Lobby")
	FOnLobbyJoinRequested on_join_requested_;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(EditDefaultsOnly, Category = "Appearance")
	FSlateBrush selected_button_brush_;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UListView> room_list_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> main_panel_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> empty_text_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> connection_text_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> status_text_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> refresh_button_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> create_button_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> join_button_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> dialog_overlay_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> dialog_title_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> dialog_status_;
	// 빠른 매칭 안내창일 때 판 가운데에 보이는 안내 문구
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> matching_text_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> name_section_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> private_section_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> password_section_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> room_name_input_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> password_input_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCheckBox> private_checkbox_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> confirm_button_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> cancel_button_;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> quick_start_button_;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> find_room_button_;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> new_room_button_;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> settings_button_;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> exit_button_;
	// 설정 메뉴 화면. 메뉴를 고를 때마다 저장된 값으로 다시 맞춘다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<USettingsWidget> settings_page_;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> menu_pages_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> search_input_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCheckBox> joinable_checkbox_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> quick_join_button_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> public_button_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> private_button_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> exit_confirm_button_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> exit_cancel_button_;

private:
	UFUNCTION()
	void SelectQuickStart();
	UFUNCTION()
	void SelectFindRoom();
	UFUNCTION()
	void SelectNewRoom();
	UFUNCTION()
	void SelectSettings();
	UFUNCTION()
	void SelectExit();
	void SelectMenu(UButton* button);


	UFUNCTION()
	void QuickJoin();
	UFUNCTION()
	void HandleSearchChanged(const FText& text);
	UFUNCTION()
	void HandleJoinableChanged(bool is_checked);
	UFUNCTION()
	void SelectPublic();
	UFUNCTION()
	void SelectPrivate();
	UFUNCTION()
	void ConfirmExit();
	void FilterRooms();
	TArray<FLobbyRoomInfo> rooms_;

	FButtonStyle menu_button_style_;
	UFUNCTION()
	void RefreshRooms();
	UFUNCTION()
	void OpenCreateDialog();
	UFUNCTION()
	void JoinSelectedRoom();
	UFUNCTION()
	void ConfirmDialog();
	UFUNCTION()
	void CloseDialog();
	UFUNCTION()
	void HandleDialogCancel();
	UFUNCTION()
	void HandlePrivateChanged(bool is_private);
	UFUNCTION()
	void HandleConnectionChanged(ENetworkConnectionState state, const FString& message);
	void HandleSelectionChanged(UObject* item);

	bool is_create_dialog_ = false;
	// 대화상자를 빠른 매칭 안내창으로 쓰는 중인지. 이때 취소 버튼은 빠른 매칭 취소 요청이다.
	bool is_matching_dialog_ = false;
	// 취소 버튼의 원래 자리. 안내창에서는 버튼이 하나라 가운데로 옮겼다가 닫을 때 되돌린다.
	FVector2D cancel_button_position_;
	int64 dialog_room_key_ = 0;
};
