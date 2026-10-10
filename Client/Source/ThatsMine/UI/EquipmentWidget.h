#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Player/equipment_display.h"
#include "EquipmentWidget.generated.h"

class UTextBlock;
class UCanvasPanel;
class UBorder;

UCLASS()
class THATSMINE_API UEquipmentWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetEquipment(const FEquipmentDisplay& display);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> crosshair_;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> name_text_;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ammo_text_;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> total_text_;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> slash_text_;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> divider_;

	FEquipmentDisplay display_;
};
