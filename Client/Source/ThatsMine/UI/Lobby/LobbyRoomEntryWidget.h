#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "LobbyRoomEntryWidget.generated.h"

class UBorder;
class UTextBlock;

UCLASS(Abstract)
class THATSMINE_API ULobbyRoomEntryWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

protected:
	virtual void NativeOnListItemObjectSet(UObject* item) override;
	virtual void NativeOnItemSelectionChanged(bool is_selected) override;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> row_background_;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> name_text_;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> count_text_;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> access_text_;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> state_text_;
};
