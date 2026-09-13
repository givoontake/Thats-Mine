#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Player/equipment_display.h"
#include "EquipmentWidget.generated.h"

class UTextBlock;
class UCanvasPanel;
class UBorder;
class UMaterialInterface;

UCLASS()
class UNREAL_PRACTICE_API UEquipmentWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetEquipment(const FEquipmentDisplay& display);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	UPROPERTY(EditDefaultsOnly, Category = "Equipment")
	TSoftObjectPtr<UMaterialInterface> frame_material_ = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Thats_Mine/UI/Equipment/M_EquipmentFrame.M_EquipmentFrame")));

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
