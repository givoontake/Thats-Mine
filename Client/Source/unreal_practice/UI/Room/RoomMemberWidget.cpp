#include "RoomMemberWidget.h"

#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Application/SlateApplication.h"

void URoomMemberWidget::SetMember(const FRoomMemberInfo* member, bool is_local)
{
	player_key_ = member ? member->player_key : 0;
	name_text_->SetText(member ? FText::FromString(member->nickname) : NSLOCTEXT("Room", "EmptySlot", "빈 자리"));
	state_text_->SetText(!member ? FText::GetEmpty() : member->is_host ? NSLOCTEXT("Room", "Host", "방장") : member->is_ready ? NSLOCTEXT("Room", "Ready", "준비 완료") : NSLOCTEXT("Room", "Waiting", "대기 중"));
	name_text_->SetColorAndOpacity(FSlateColor(member ? FLinearColor(.97f, .90f, .77f) : FLinearColor(.45f, .40f, .33f)));
	state_text_->SetColorAndOpacity(FSlateColor(member && member->is_ready ? FLinearColor(.52f, .8f, .42f) : FLinearColor(.87f, .68f, .36f)));
	background_->SetBrushColor(is_local ? FLinearColor::White : FLinearColor(.72f, .72f, .72f, 1.0f));
}

void URoomMemberWidget::SetCanKick(bool can_kick)
{
	can_kick_ = can_kick;
	SetVisibility(ESlateVisibility::Visible);
	SetToolTipText(can_kick ? NSLOCTEXT("Room", "KickHint", "우클릭: 참가자 관리") : FText::GetEmpty());
}

FReply URoomMemberWidget::NativeOnMouseButtonDown(const FGeometry& geometry, const FPointerEvent& event)
{
	if (can_kick_ && event.GetEffectingButton() == EKeys::RightMouseButton) {
		const int64 selected_player = player_key_;
		FMenuBuilder menu(true, nullptr);
		menu.AddMenuEntry(NSLOCTEXT("Room", "Kick", "강퇴"), FText::GetEmpty(), FSlateIcon(), FUIAction(FExecuteAction::CreateWeakLambda(this, [this, selected_player]() { if (can_kick_ && selected_player == player_key_) on_kick_requested_.ExecuteIfBound(selected_player); })));
		FSlateApplication::Get().PushMenu(TakeWidget(), FWidgetPath(), menu.MakeWidget(), event.GetScreenSpacePosition(), FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(geometry, event);
}
