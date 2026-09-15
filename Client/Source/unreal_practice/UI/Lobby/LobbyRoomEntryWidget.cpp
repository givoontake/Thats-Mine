#include "LobbyRoomEntryWidget.h"

#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "LobbyRoomItem.h"

void ULobbyRoomEntryWidget::NativeOnListItemObjectSet(UObject* item)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(item);
	const ULobbyRoomItem* room_item = Cast<ULobbyRoomItem>(item);
	if (!room_item)
	{
		return;
	}
	const FLobbyRoomInfo& room = room_item->room_;
	name_text_->SetText(FText::AsCultureInvariant(room.room_name));
	count_text_->SetText(FText::Format(NSLOCTEXT("Lobby", "PlayerCount", "{0} / {1}"), room.player_count, room.max_player_count));
	access_text_->SetText(room.is_private ? NSLOCTEXT("Lobby", "Private", "비공개") : NSLOCTEXT("Lobby", "Public", "공개"));
	state_text_->SetText(room.is_playing ? NSLOCTEXT("Lobby", "Playing", "진행 중") : NSLOCTEXT("Lobby", "Waiting", "대기 중"));
	state_text_->SetColorAndOpacity(FSlateColor(room.is_playing ? FLinearColor(0.8f, 0.5f, 0.2f) : FLinearColor(0.4f, 0.75f, 0.35f)));
	NativeOnItemSelectionChanged(IsListItemSelected());
}

void ULobbyRoomEntryWidget::NativeOnItemSelectionChanged(bool is_selected)
{
	IUserObjectListEntry::NativeOnItemSelectionChanged(is_selected);
	row_background_->SetBrushColor(is_selected ? FLinearColor(0.24f, 0.15f, 0.045f, 1.0f) : FLinearColor(0.04f, 0.034f, 0.025f, 0.94f));
}
