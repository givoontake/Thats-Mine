#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "room_member_info.h"
#include "RoomMemberWidget.generated.h"

class UBorder;
class UTextBlock;

DECLARE_DELEGATE_OneParam(FOnMemberKickRequested, int64);

UCLASS(Abstract)
class UNREAL_PRACTICE_API URoomMemberWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetMember(const FRoomMemberInfo* member, bool is_local);
	void SetCanKick(bool can_kick);
	FOnMemberKickRequested on_kick_requested_;

protected:
	virtual FReply NativeOnMouseButtonDown(const FGeometry& geometry, const FPointerEvent& event) override;
	int64 player_key_ = 0;
	bool can_kick_ = false;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> name_text_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> state_text_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> background_;
};
