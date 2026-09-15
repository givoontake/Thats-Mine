#include "RoomMemberWidget.h"

#include "Components/Border.h"
#include "Components/TextBlock.h"

void URoomMemberWidget::SetMember(const FRoomMemberInfo* member, bool is_local)
{
	name_text_->SetText(member ? FText::FromString(member->nickname) : NSLOCTEXT("Room", "EmptySlot", "빈 자리"));
	state_text_->SetText(!member ? FText::GetEmpty() : member->is_host ? NSLOCTEXT("Room", "Host", "방장") : member->is_ready ? NSLOCTEXT("Room", "Ready", "준비 완료") : NSLOCTEXT("Room", "Waiting", "대기 중"));
	name_text_->SetColorAndOpacity(FSlateColor(member ? FLinearColor(.97f, .90f, .77f) : FLinearColor(.45f, .40f, .33f)));
	state_text_->SetColorAndOpacity(FSlateColor(member && member->is_ready ? FLinearColor(.52f, .8f, .42f) : FLinearColor(.87f, .68f, .36f)));
	background_->SetBrushColor(is_local ? FLinearColor::White : FLinearColor(.72f, .72f, .72f, 1.0f));
}
