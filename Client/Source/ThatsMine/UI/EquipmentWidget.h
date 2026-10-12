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
	// 패널 폭에 대한 구분선 위치 비율과, 구분선에서 이름·탄 숫자까지의 간격
	static constexpr float DIVIDER_ANCHOR = 0.52f;
	static constexpr float DIVIDER_GAP = 32.0f;

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
