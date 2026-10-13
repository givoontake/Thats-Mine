#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BlockBoardWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UTextBlock;
class UTexture2D;

// 인벤토리 장비 탭의 블록 칸. 왼쪽은 지도 사진을 8×8칸으로 나눈 배제 판이고, 오른쪽은 가졌고 아직 쓰지 않은 블록이다.
// 블록을 마우스 왼쪽 버튼으로 끌어 판에 놓으면 칸에 맞춰 붙고, 놓을 수 있는 자리(모든 칸이 판 안이고 배제되지 않음)면 초록, 아니면 빨강으로 보인다.
// 끄는 중 오른쪽 버튼·휠·R키로 시계 방향으로 돌린다. 놓을 수 있는 자리에서 버튼을 떼면 사용할지 묻고, 사용을 누르면 서버에 보낸다.
UCLASS()
class THATSMINE_API UBlockBoardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UBlockBoardWidget(const FObjectInitializer& object_initializer);

	// 위젯 크기. 인벤토리가 이 크기의 칸에 놓는다.
	static constexpr double WIDTH = 1000.0;
	static constexpr double HEIGHT = 420.0;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual int32 NativePaint(const FPaintArgs& args, const FGeometry& geometry, const FSlateRect& culling_rect, FSlateWindowElementList& draw_elements, int32 layer_id, const FWidgetStyle& widget_style, bool is_parent_enabled) const override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& geometry, const FPointerEvent& event) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& geometry, const FPointerEvent& event) override;
	virtual FReply NativeOnMouseMove(const FGeometry& geometry, const FPointerEvent& event) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& geometry, const FPointerEvent& event) override;
	virtual FReply NativeOnKeyDown(const FGeometry& geometry, const FKeyEvent& event) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& event) override;

private:
	// 판은 왼쪽 위에 놓은 정사각형이고, 블록 칸은 판 오른쪽에 가로 SLOT_COLUMNS개씩 줄지어 놓는다.
	static constexpr double BOARD_SIZE = HEIGHT;
	static constexpr double LIST_LEFT = 450.0;
	static constexpr double LIST_TOP = 70.0;
	static constexpr int32 SLOT_COLUMNS = 6;
	static constexpr double SLOT_SIZE = 85.0;
	static constexpr double SLOT_STEP = 87.0;
	// 블록 칸 안에 그리는 블록 한 칸 크기
	static constexpr double SLOT_CELL_SIZE = 15.0;

	// 놓을 자리. 블록을 돌린 모양의 가장 작은 열과 행이 (column, row) 칸에 온다.
	struct FBlockPlacement
	{
		int32 piece = INDEX_NONE;
		int32 rotation = 0;
		int32 column = 0;
		int32 row = 0;
	};

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> map_texture_;

	// 배제 칸 텍스처. 배제된 곳은 알파 255, 나머지는 0이다.
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> exclusion_texture_;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> block_count_;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> empty_text_;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> confirm_panel_;

	TArray<int32> usable_blocks_;
	uint64 exclusion_board_ = 0;
	// 끄는 중인 블록과 돌린 수. 끄는 중이 아니면 piece가 INDEX_NONE이다.
	FBlockPlacement drag_;
	FVector2D cursor_ = FVector2D::ZeroVector;
	// 사용할지 묻는 중인 자리. 묻는 중이 아니면 piece가 INDEX_NONE이다.
	FBlockPlacement pending_;

	UFUNCTION()
	void ConfirmUse();
	UFUNCTION()
	void CancelUse();
	void RefreshBlocks();
	void RefreshExclusion();
	// 끄는 블록을 커서 아래 칸에 맞춘 자리. 블록 가운데가 커서에 오게 한다.
	FBlockPlacement GetSnappedPlacement() const;
	bool IsOverBoard(const FVector2D& position) const;
	bool CanPlace(const FBlockPlacement& placement) const;
	FVector2D GetSlotPosition(int32 index) const;
	void RotateDrag();
	void CancelDrag();
};
