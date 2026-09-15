#include "RoomWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "RoomMemberWidget.h"

void URoomWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	action_style_ = action_button_->GetStyle();
	action_button_->OnClicked.AddDynamic(this, &URoomWidget::HandleAction);
	leave_button_->OnClicked.AddDynamic(this, &URoomWidget::ShowLeaveDialog);
	leave_confirm_button_->OnClicked.AddDynamic(this, &URoomWidget::ConfirmLeave);
	leave_cancel_button_->OnClicked.AddDynamic(this, &URoomWidget::CancelLeave);
	leave_dialog_->SetVisibility(ESlateVisibility::Collapsed);
	UClass* member_class = member_widget_class_.LoadSynchronous();
	if (!member_class)
	{
		UE_LOG(LogTemp, Error, TEXT("Unable to load room member widget."));
		return;
	}
	for (int32 index = 0; index < 20; ++index)
	{
		URoomMemberWidget* member = CreateWidget<URoomMemberWidget>(GetOwningPlayer(), member_class);
		if (!member)
		{
			return;
		}
		UUniformGridSlot* slot = members_grid_->AddChildToUniformGrid(member, index % 10, index / 10);
		slot->SetHorizontalAlignment(HAlign_Fill);
		slot->SetVerticalAlignment(VAlign_Fill);
		member_widgets_.Add(member);
		member->SetMember(nullptr, false);
	}
}

void URoomWidget::SetRoom(const FLobbyRoomInfo& room, const TArray<FRoomMemberInfo>& members, int64 local_player_key, bool can_start)
{
	room_name_text_->SetText(FText::FromString(room.room_name));
	room_info_text_->SetText(FText::Format(NSLOCTEXT("Room", "Info", "{0}  ·  {1} / {2}"), room.is_private ? NSLOCTEXT("Room", "Private", "비공개") : NSLOCTEXT("Room", "Public", "공개"), room.player_count, room.max_player_count));
	const FRoomMemberInfo* local = members.FindByPredicate([local_player_key](const FRoomMemberInfo& member) { return member.player_key == local_player_key; });
	is_host_ = local && local->is_host;
	is_ready_ = local && local->is_ready;
	for (int32 index = 0; index < member_widgets_.Num(); ++index)
	{
		const FRoomMemberInfo* member = members.IsValidIndex(index) ? &members[index] : nullptr;
		member_widgets_[index]->SetMember(member, member && member->player_key == local_player_key);
	}
	action_text_->SetText(is_host_ ? NSLOCTEXT("Room", "Start", "게임 시작") : is_ready_ ? NSLOCTEXT("Room", "CancelReady", "준비 취소") : NSLOCTEXT("Room", "ReadyAction", "준비하기"));
	action_button_->SetIsEnabled(local && (!is_host_ || can_start));
	FButtonStyle style = action_style_;
	if (!is_host_ && is_ready_)
	{
		style.Normal.SetResourceObject(selected_button_brush_.GetResourceObject());
		style.NormalForeground = style.PressedForeground;
		style.Hovered = style.Normal;
		style.HoveredForeground = style.PressedForeground;
	}
	action_button_->SetStyle(style);
	status_text_->SetText(is_host_ ? (can_start ? NSLOCTEXT("Room", "HostCanStart", "게임을 시작할 수 있습니다.") : NSLOCTEXT("Room", "HostWait", "참가자의 준비를 기다리는 중입니다.")) : is_ready_ ? NSLOCTEXT("Room", "ReadyStatus", "준비 완료 · 방장의 시작을 기다립니다.") : NSLOCTEXT("Room", "PrepareStatus", "준비되면 준비하기를 눌러 주세요."));
}

void URoomWidget::HandleAction()
{
	if (is_host_)
	{
		on_start_requested_.Broadcast();
	}
	else
	{
		on_ready_requested_.Broadcast(!is_ready_);
	}
}

void URoomWidget::ShowLeaveDialog()
{
	leave_dialog_->SetVisibility(ESlateVisibility::Visible);
	leave_cancel_button_->SetKeyboardFocus();
}

void URoomWidget::CancelLeave()
{
	leave_dialog_->SetVisibility(ESlateVisibility::Collapsed);
	leave_button_->SetKeyboardFocus();
}

void URoomWidget::ConfirmLeave()
{
	on_leave_requested_.Broadcast();
}
