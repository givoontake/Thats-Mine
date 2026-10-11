#include "LobbyWidget.h"

#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/EditableTextBox.h"
#include "Components/ListView.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/GameInstance.h"
#include "Flow/ClientStateSubsystem.h"
#include "LobbyRoomItem.h"
#include "Network/GameNetworkSubsystem.h"

#define LOCTEXT_NAMESPACE "Lobby"

void ULobbyWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	SetDesiredFocusWidget(quick_start_button_ ? quick_start_button_.Get() : refresh_button_.Get());
	if (quick_start_button_ && find_room_button_ && new_room_button_ && settings_button_ && exit_button_)
	{
		menu_button_style_ = quick_start_button_->GetStyle();
		quick_start_button_->OnClicked.AddDynamic(this, &ULobbyWidget::SelectQuickStart);
		find_room_button_->OnClicked.AddDynamic(this, &ULobbyWidget::SelectFindRoom);
		new_room_button_->OnClicked.AddDynamic(this, &ULobbyWidget::SelectNewRoom);
		settings_button_->OnClicked.AddDynamic(this, &ULobbyWidget::SelectSettings);
		exit_button_->OnClicked.AddDynamic(this, &ULobbyWidget::SelectExit);
	}
	refresh_button_->OnClicked.AddDynamic(this, &ULobbyWidget::RefreshRooms);
	create_button_->OnClicked.AddDynamic(this, &ULobbyWidget::OpenCreateDialog);
	join_button_->OnClicked.AddDynamic(this, &ULobbyWidget::JoinSelectedRoom);
	confirm_button_->OnClicked.AddDynamic(this, &ULobbyWidget::ConfirmDialog);
	cancel_button_->OnClicked.AddDynamic(this, &ULobbyWidget::CloseDialog);
	private_checkbox_->OnCheckStateChanged.AddDynamic(this, &ULobbyWidget::HandlePrivateChanged);
	room_list_->OnItemSelectionChanged().AddUObject(this, &ULobbyWidget::HandleSelectionChanged);
	quick_join_button_->OnClicked.AddDynamic(this, &ULobbyWidget::QuickJoin);
	search_input_->OnTextChanged.AddDynamic(this, &ULobbyWidget::HandleSearchChanged);
	joinable_checkbox_->OnCheckStateChanged.AddDynamic(this, &ULobbyWidget::HandleJoinableChanged);
	public_button_->OnClicked.AddDynamic(this, &ULobbyWidget::SelectPublic);
	private_button_->OnClicked.AddDynamic(this, &ULobbyWidget::SelectPrivate);
	exit_confirm_button_->OnClicked.AddDynamic(this, &ULobbyWidget::ConfirmExit);
	exit_cancel_button_->OnClicked.AddDynamic(this, &ULobbyWidget::SelectQuickStart);
	SelectQuickStart();
	password_input_->SetIsPassword(true);
	dialog_overlay_->SetVisibility(ESlateVisibility::Collapsed);
	join_button_->SetIsEnabled(false);
}

void ULobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();
	room_list_->SetScrollbarVisibility(ESlateVisibility::Visible);
	if (GetGameInstance()->GetSubsystem<UClientStateSubsystem>()->IsLocalMode())
	{
		connection_text_->SetText(LOCTEXT("LocalMode", "로컬 테스트 모드"));
		connection_text_->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.7f, 0.25f)));
		return;
	}
	UGameNetworkSubsystem* network = GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>();
	network->on_connection_state_changed_.AddUniqueDynamic(this, &ULobbyWidget::HandleConnectionChanged);
	HandleConnectionChanged(network->GetConnectionState(), network->GetConnectionMessage());
}

void ULobbyWidget::NativeDestruct()
{
	if (UGameInstance* game_instance = GetGameInstance())
	{
		if (UGameNetworkSubsystem* network = game_instance->GetSubsystem<UGameNetworkSubsystem>())
		{
			network->on_connection_state_changed_.RemoveDynamic(this, &ULobbyWidget::HandleConnectionChanged);
		}
	}
	password_input_->SetText(FText::GetEmpty());
	Super::NativeDestruct();
}

void ULobbyWidget::SetRooms(const TArray<FLobbyRoomInfo>& rooms)
{
	rooms_ = rooms;
	FilterRooms();
}

void ULobbyWidget::FilterRooms()
{
	const FString search = search_input_->GetText().ToString().TrimStartAndEnd();
	const ULobbyRoomItem* previous = room_list_->GetSelectedItem<ULobbyRoomItem>();
	const bool has_selection = previous != nullptr;
	const int64 previous_key = previous ? previous->room_.room_key : 0;
	TArray<UObject*> items;
	items.Reserve(rooms_.Num());
	ULobbyRoomItem* selected = nullptr;
	for (const FLobbyRoomInfo& room : rooms_)
	{
		if ((!search.IsEmpty() && !room.room_name.Contains(search, ESearchCase::IgnoreCase)) || (joinable_checkbox_->IsChecked() && (room.is_playing || room.player_count >= room.max_player_count)))
		{
			continue;
		}
		ULobbyRoomItem* item = NewObject<ULobbyRoomItem>(this);
		item->room_ = room;
		items.Add(item);
		if (has_selection && room.room_key == previous_key)
		{
			selected = item;
		}
	}
	room_list_->SetListItems(items);
	if (selected)
	{
		room_list_->SetSelectedItem(selected);
	}
	HandleSelectionChanged(selected);
	empty_text_->SetText(rooms_.IsEmpty() ? LOCTEXT("Empty", "생성된 방이 없습니다.") : LOCTEXT("NoMatches", "조건에 맞는 방이 없습니다."));
	empty_text_->SetVisibility(items.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void ULobbyWidget::ShowStatus(const FText& message)
{
	status_text_->SetText(message);
}

void ULobbyWidget::HandleSelectionChanged(UObject* item)
{
	const ULobbyRoomItem* selected = Cast<ULobbyRoomItem>(item);
	join_button_->SetIsEnabled(selected && !selected->room_.is_playing && selected->room_.player_count < selected->room_.max_player_count);
}

void ULobbyWidget::RefreshRooms()
{
	if (on_refresh_requested_.IsBound())
	{
		on_refresh_requested_.Broadcast();
	}
	else
	{
		ShowStatus(LOCTEXT("RefreshUnavailable", "방 목록 조회를 준비 중입니다."));
	}
}

void ULobbyWidget::OpenCreateDialog()
{
	SelectMenu(new_room_button_);
	is_create_dialog_ = true;
	dialog_title_->SetText(LOCTEXT("CreateRoom", "방 만들기"));
	dialog_status_->SetText(FText::GetEmpty());
	room_name_input_->SetText(FText::GetEmpty());
	password_input_->SetText(FText::GetEmpty());
	SelectPublic();
	name_section_->SetVisibility(ESlateVisibility::Visible);
	private_section_->SetVisibility(ESlateVisibility::Visible);
	password_section_->SetVisibility(ESlateVisibility::Collapsed);
	main_panel_->SetIsEnabled(false);
	dialog_overlay_->SetVisibility(ESlateVisibility::Visible);
	room_name_input_->SetKeyboardFocus();
}

void ULobbyWidget::JoinSelectedRoom()
{
	const ULobbyRoomItem* selected = room_list_->GetSelectedItem<ULobbyRoomItem>();
	if (!selected || selected->room_.is_playing || selected->room_.player_count >= selected->room_.max_player_count)
	{
		ShowStatus(LOCTEXT("SelectJoinableRoom", "입장 가능한 방을 선택해 주세요."));
		return;
	}
	if (!selected->room_.is_private)
	{
		if (on_join_requested_.IsBound())
		{
			on_join_requested_.Broadcast(selected->room_.room_key, FString());
		}
		else
		{
			ShowStatus(LOCTEXT("JoinUnavailable", "방 입장 서비스를 준비 중입니다."));
		}
		return;
	}
	menu_pages_->SetActiveWidgetIndex(2);
	is_create_dialog_ = false;
	dialog_room_key_ = selected->room_.room_key;
	dialog_title_->SetText(LOCTEXT("RoomPassword", "비공개 방 입장"));
	dialog_status_->SetText(FText::GetEmpty());
	name_section_->SetVisibility(ESlateVisibility::Collapsed);
	private_section_->SetVisibility(ESlateVisibility::Collapsed);
	password_section_->SetVisibility(ESlateVisibility::Visible);
	password_input_->SetText(FText::GetEmpty());
	main_panel_->SetIsEnabled(false);
	dialog_overlay_->SetVisibility(ESlateVisibility::Visible);
	password_input_->SetKeyboardFocus();
}

void ULobbyWidget::ConfirmDialog()
{
	const bool is_private = !is_create_dialog_ || private_checkbox_->IsChecked();
	const FString name = room_name_input_->GetText().ToString().TrimStartAndEnd();
	const FString password = is_private ? password_input_->GetText().ToString() : FString();
	if (is_create_dialog_ && name.IsEmpty())
	{
		dialog_status_->SetText(LOCTEXT("MissingRoomName", "방 이름을 입력해 주세요."));
		room_name_input_->SetKeyboardFocus();
		return;
	}
	if (is_private && password.IsEmpty())
	{
		dialog_status_->SetText(LOCTEXT("MissingPassword", "비밀번호를 입력해 주세요."));
		password_input_->SetKeyboardFocus();
		return;
	}
	if (is_create_dialog_)
	{
		CloseDialog();
		if (on_create_requested_.IsBound())
		{
			on_create_requested_.Broadcast(name, is_private, password);
		}
		else
		{
			ShowStatus(LOCTEXT("CreateUnavailable", "방 생성 서비스를 준비 중입니다."));
		}
		return;
	}
	const ULobbyRoomItem* selected = room_list_->GetSelectedItem<ULobbyRoomItem>();
	if (!selected || selected->room_.is_playing || selected->room_.player_count >= selected->room_.max_player_count)
	{
		CloseDialog();
		ShowStatus(LOCTEXT("RoomChanged", "방 정보가 변경되었습니다. 다시 선택해 주세요."));
		return;
	}
	if (selected->room_.room_key != dialog_room_key_)
	{
		CloseDialog();
		ShowStatus(LOCTEXT("RoomChanged", "방 정보가 변경되었습니다. 다시 선택해 주세요."));
		return;
	}
	const int64 room_key = selected->room_.room_key;
	CloseDialog();
	if (on_join_requested_.IsBound())
	{
		on_join_requested_.Broadcast(room_key, password);
	}
	else
	{
		ShowStatus(LOCTEXT("JoinUnavailable", "방 입장 서비스를 준비 중입니다."));
	}
}

void ULobbyWidget::CloseDialog()
{
	main_panel_->SetIsEnabled(true);
	password_input_->SetText(FText::GetEmpty());
	dialog_overlay_->SetVisibility(ESlateVisibility::Collapsed);
	SelectMenu(find_room_button_);
	refresh_button_->SetKeyboardFocus();
}

void ULobbyWidget::HandlePrivateChanged(bool is_private)
{
	for (UButton* button : {public_button_.Get(), private_button_.Get()})
	{
		FButtonStyle style = button->GetStyle();
		style.Normal = menu_button_style_.Normal;
		style.Normal.ImageSize *= style.Hovered.ImageSize.Y / style.Normal.ImageSize.Y;
		style.NormalForeground = menu_button_style_.NormalForeground;
		style.Hovered.SetResourceObject(menu_button_style_.Hovered.GetResourceObject());
		style.HoveredForeground = menu_button_style_.HoveredForeground;
		if ((button == private_button_) == is_private)
		{
			style.Normal.SetResourceObject(selected_button_brush_.GetResourceObject());
			style.NormalForeground = style.PressedForeground;
			style.Hovered = style.Normal;
			style.HoveredForeground = style.PressedForeground;
		}
		button->SetStyle(style);
	}
	password_section_->SetVisibility(is_private ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (!is_private)
	{
		password_input_->SetText(FText::GetEmpty());
	}
}

void ULobbyWidget::HandleConnectionChanged(ENetworkConnectionState state, const FString& message)
{
	const bool is_connected = state == ENetworkConnectionState::CONNECTED;
	connection_text_->SetText(is_connected ? LOCTEXT("Connected", "서버 연결됨") : LOCTEXT("Disconnected", "서버 연결 안 됨"));
	connection_text_->SetColorAndOpacity(FSlateColor(is_connected ? FLinearColor(0.4f, 0.75f, 0.35f) : FLinearColor(0.9f, 0.45f, 0.25f)));
	if (is_connected && room_list_->GetNumItems() == 0)
	{
		empty_text_->SetText(LOCTEXT("RequestRoomList", "방 목록을 불러와 주세요."));
	}
	if (!is_connected)
	{
		if (dialog_overlay_->GetVisibility() != ESlateVisibility::Collapsed)
		{
			CloseDialog();
		}
		SetRooms(TArray<FLobbyRoomInfo>());
		empty_text_->SetText(LOCTEXT("NoConnection", "서버에 연결되지 않았습니다."));
	}
}

void ULobbyWidget::SelectMenu(UButton* button)
{
	main_panel_->SetIsEnabled(true);
	dialog_overlay_->SetVisibility(ESlateVisibility::Collapsed);
	password_input_->SetText(FText::GetEmpty());
	int32 index = 0;
	for (UButton* menu_button : {quick_start_button_.Get(), find_room_button_.Get(), new_room_button_.Get(), settings_button_.Get(), exit_button_.Get()})
	{
		if (!menu_button)
		{
			continue;
		}
		FButtonStyle style = menu_button->GetStyle();
		style.Normal = menu_button_style_.Normal;
		style.Normal.ImageSize *= style.Hovered.ImageSize.Y / style.Normal.ImageSize.Y;
		style.NormalForeground = menu_button_style_.NormalForeground;
		style.Hovered.SetResourceObject(menu_button_style_.Hovered.GetResourceObject());
		style.HoveredForeground = menu_button_style_.HoveredForeground;
		if (menu_button == button)
		{
			style.Normal.SetResourceObject(selected_button_brush_.GetResourceObject());
			style.NormalForeground = style.PressedForeground;
			style.Hovered = style.Normal;
			style.HoveredForeground = style.PressedForeground;
			menu_pages_->SetActiveWidgetIndex(index);
		}
		menu_button->SetStyle(style);
		++index;
	}
}

void ULobbyWidget::SelectQuickStart()
{
	SelectMenu(quick_start_button_);
}

void ULobbyWidget::SelectFindRoom()
{
	SelectMenu(find_room_button_);
}

void ULobbyWidget::SelectNewRoom()
{
	OpenCreateDialog();
}

void ULobbyWidget::SelectSettings()
{
	SelectMenu(settings_button_);
}

void ULobbyWidget::SelectExit()
{
	SelectMenu(exit_button_);
}


void ULobbyWidget::QuickJoin()
{
	if (on_quick_join_requested_.IsBound()) { on_quick_join_requested_.Broadcast(); return; }
	for (const FLobbyRoomInfo& room : rooms_)
	{
		if (!room.is_private && !room.is_playing && room.player_count < room.max_player_count)
		{
			if (on_join_requested_.IsBound())
			{
				on_join_requested_.Broadcast(room.room_key, FString());
			}
			else
			{
				ShowStatus(LOCTEXT("JoinUnavailable", "방 입장 서비스를 준비 중입니다."));
			}
			return;
		}
	}
	ShowStatus(LOCTEXT("NoQuickRoom", "참가 가능한 공개 방이 없습니다. 방 찾기에서 새로고침해 주세요."));
}

void ULobbyWidget::HandleSearchChanged(const FText& text)
{
	FilterRooms();
}

void ULobbyWidget::HandleJoinableChanged(bool is_checked)
{
	FilterRooms();
}

void ULobbyWidget::SelectPublic()
{
	private_checkbox_->SetIsChecked(false);
	HandlePrivateChanged(false);
}

void ULobbyWidget::SelectPrivate()
{
	private_checkbox_->SetIsChecked(true);
	HandlePrivateChanged(true);
}

void ULobbyWidget::ConfirmExit()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

#undef LOCTEXT_NAMESPACE

void ULobbyWidget::SetPending(bool is_pending)
{
	quick_join_button_->SetIsEnabled(!is_pending);
	refresh_button_->SetIsEnabled(!is_pending);
	create_button_->SetIsEnabled(!is_pending);
	confirm_button_->SetIsEnabled(!is_pending);
	if (is_pending) join_button_->SetIsEnabled(false);
	else HandleSelectionChanged(room_list_->GetSelectedItem<UObject>());
}
