#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Player/weapon_type.h"
#include "InventoryWidget.generated.h"

class APlayerCharacter;
class UCanvasPanel;
class UImage;
class UScrollBox;
class UTextBlock;
class UTexture2D;

DECLARE_DELEGATE(FOnInventoryCloseRequested);

UCLASS()
class THATSMINE_API UInventoryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UInventoryWidget(const FObjectInitializer& object_initializer);
	FOnInventoryCloseRequested on_close_requested_;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& geometry, const FKeyEvent& event) override;
	virtual FReply NativeOnKeyDown(const FGeometry& geometry, const FKeyEvent& event) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& geometry, const FPointerEvent& event) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& geometry, const FPointerEvent& event) override;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TSoftObjectPtr<UTexture2D> frame_texture_;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory")
	TArray<TSoftObjectPtr<UTexture2D>> equipment_icons_;

private:
	static constexpr EWeaponType EQUIPMENT_ORDER[] = { EWeaponType::SNIPER, EWeaponType::PISTOL, EWeaponType::PICKAXE };

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> equipment_frames_;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> equipment_names_;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> equipment_ammo_;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> hint_count_;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> hint_list_;

	TWeakObjectPtr<APlayerCharacter> character_;
	FDelegateHandle equipment_changed_handle_;
	FDelegateHandle hints_changed_handle_;

	UTextBlock* AddText(UCanvasPanel* canvas, const FText& text, int32 size, const FVector2D& position, const FVector2D& dimensions);
	UImage* AddPanel(UCanvasPanel* canvas, const FVector2D& position, const FVector2D& dimensions);
	void RefreshEquipment();
	void RefreshHints();
};
