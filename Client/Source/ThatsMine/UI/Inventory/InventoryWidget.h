#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateTypes.h"
#include "Player/inventory_hint.h"
#include "Player/weapon_type.h"
#include "InventoryWidget.generated.h"

class APlayerCharacter;
class UBorder;
class UButton;
class UCanvasPanel;
class UImage;
class UScrollBox;
class UTextBlock;
class UTexture2D;
class UWidgetSwitcher;
class UWrapBox;

DECLARE_DELEGATE(FOnInventoryCloseRequested);

// 인벤토리. 장비·아이템·힌트·블록 탭으로 나누어 보이고, 열 때마다 힌트 탭을 먼저 보인다.
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
	static constexpr EWeaponType EQUIPMENT_ORDER[] = { EWeaponType::SNIPER, EWeaponType::PISTOL, EWeaponType::PICKAXE, EWeaponType::MAGNIFIER };
	// 탭 순서. 탭 버튼과 탭 화면이 같은 순서다.
	static constexpr int32 EQUIPMENT_TAB = 0;
	static constexpr int32 ITEM_TAB = 1;
	static constexpr int32 HINT_TAB = 2;
	static constexpr int32 BLOCK_TAB = 3;

	// 탭 버튼 모양과 고른 탭의 배경. 로비 메뉴 버튼과 같은 나무 모양이고, 화면을 만들 때 재질을 불러 채운다.
	UPROPERTY(Transient)
	FButtonStyle tab_style_;
	UPROPERTY(Transient)
	FSlateBrush selected_tab_brush_;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> tab_buttons_;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetSwitcher> pages_;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> equipment_frames_;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> equipment_names_;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> equipment_ammo_;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> item_count_;

	UPROPERTY(Transient)
	TObjectPtr<UWrapBox> item_list_;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> hint_count_;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> hint_list_;

	// 힌트 필터 버튼. 전체, 위치, 단서 순서다.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> filter_buttons_;

	// 보이는 힌트 분류. 비어 있으면 전부 보인다.
	TOptional<EHintCategory> hint_filter_;

	TWeakObjectPtr<APlayerCharacter> character_;
	FDelegateHandle equipment_changed_handle_;
	FDelegateHandle hints_changed_handle_;
	FDelegateHandle items_changed_handle_;

	UFUNCTION()
	void SelectEquipmentTab();
	UFUNCTION()
	void SelectItemTab();
	UFUNCTION()
	void SelectHintTab();
	UFUNCTION()
	void SelectBlockTab();
	void SelectTab(int32 tab);
	UFUNCTION()
	void ShowAllHints();
	UFUNCTION()
	void ShowLocationHints();
	UFUNCTION()
	void ShowClueHints();
	void SelectHintFilter(TOptional<EHintCategory> filter);
	void ToggleFavorite(int32 hint_index);
	// 버튼에 고른 모양(선택 배경과 눌림 글자색)이나 기본 모양을 입힌다.
	void ApplyButtonStyle(UButton* button, bool is_selected);
	UTextBlock* AddText(UCanvasPanel* canvas, const FText& text, int32 size, const FVector2D& position, const FVector2D& dimensions);
	UImage* AddPanel(UCanvasPanel* canvas, const FVector2D& position, const FVector2D& dimensions);
	void RefreshEquipment();
	void RefreshItems();
	void RefreshHints();
	// 아이템 한 줄. 등급 배지와 문장을 담는다.
	UBorder* CreateGradeRow(EHintGrade grade, const FText& text, int32 font_size);
	// 힌트 한 줄. 즐겨찾기 별, 분류 표시, 문장, 새 힌트 표시를 담는다. 줄 모양은 아이템 줄과 같다.
	UBorder* CreateHintRow(int32 hint_index, const FInventoryHint& hint);
};
