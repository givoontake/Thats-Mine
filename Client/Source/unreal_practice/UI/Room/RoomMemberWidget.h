#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "room_member_info.h"
#include "RoomMemberWidget.generated.h"

class UBorder;
class UTextBlock;

UCLASS(Abstract)
class UNREAL_PRACTICE_API URoomMemberWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetMember(const FRoomMemberInfo* member, bool is_local);

protected:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> name_text_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> state_text_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> background_;
};
