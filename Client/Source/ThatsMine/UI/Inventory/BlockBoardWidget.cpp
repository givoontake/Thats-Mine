#include "BlockBoardWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Interaction/BlockSubsystem.h"
#include "Interaction/HintSubsystem.h"
#include "Materials/MaterialInterface.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "block_pieces.h"
#include "UI/game_font.h"

static const FLinearColor TEXT_COLOR(0.96f, 0.90f, 0.78f);
static const FLinearColor PIECE_COLOR(1.0f, 0.61f, 0.12f);
static const FLinearColor VALID_COLOR(0.30f, 0.90f, 0.40f, 0.65f);
static const FLinearColor INVALID_COLOR(0.95f, 0.25f, 0.20f, 0.65f);
// 배제 칸 텍스처와 배제 판 칸에 곱하는 어둡기. 배제 판 칸은 두 번 어두워진다.
static constexpr float EXCLUSION_SHADE_OPACITY = 0.3f;
static constexpr float BOARD_SHADE_OPACITY = 0.4f;

// 나무 버튼 재질 하나로 버튼 크기의 그림 붓을 만든다.
static FSlateBrush MakeButtonBrush(const TCHAR* material_path)
{
	FSlateBrush brush;
	brush.SetResourceObject(LoadObject<UMaterialInterface>(nullptr, material_path));
	brush.ImageSize = FVector2D(200.0, 52.0);
	return brush;
}

// 블록 모양이 차지하는 열 수와 행 수
static FIntPoint GetPieceSize(const BlockPiece& piece)
{
	FIntPoint size(0, 0);
	for (int32 cell = 0; cell < piece.cell_count; ++cell)
	{
		size.X = FMath::Max(size.X, piece.cells[cell][0] + 1);
		size.Y = FMath::Max(size.Y, piece.cells[cell][1] + 1);
	}
	return size;
}

UBlockBoardWidget::UBlockBoardWidget(const FObjectInitializer& object_initializer)
	: Super(object_initializer)
{
	SetIsFocusable(true);
	SetVisibility(ESlateVisibility::Visible);
}

TSharedRef<SWidget> UBlockBoardWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		map_texture_ = LoadObject<UTexture2D>(nullptr, TEXT("/Game/Thats_Mine/UI/Map/T_WorldMap.T_WorldMap"));
		UCanvasPanel* root = WidgetTree->ConstructWidget<UCanvasPanel>();
		root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		WidgetTree->RootWidget = root;
		const auto add_text = [this](UCanvasPanel* canvas, const FText& text, int32 size, const FLinearColor& color, const FVector2D& position, const FVector2D& dimensions)
		{
			UTextBlock* label = WidgetTree->ConstructWidget<UTextBlock>();
			label->SetText(text);
			label->SetFont(GetGameFont(size));
			label->SetColorAndOpacity(color);
			label->SetVisibility(ESlateVisibility::HitTestInvisible);
			UCanvasPanelSlot* slot = canvas->AddChildToCanvas(label);
			slot->SetPosition(position);
			slot->SetSize(dimensions);
			return label;
		};
		block_count_ = add_text(root, FText::GetEmpty(), 26, TEXT_COLOR, FVector2D(LIST_LEFT, 0.0), FVector2D(WIDTH - LIST_LEFT, 38.0));
		add_text(root, NSLOCTEXT("Block", "Guide", "블록을 끌어 지도에 놓으세요 · 끄는 중 우클릭·휠·R로 회전"), 16, FLinearColor(0.65f, 0.60f, 0.51f), FVector2D(LIST_LEFT, 38.0), FVector2D(WIDTH - LIST_LEFT, 50.0))->SetAutoWrapText(true);
		empty_text_ = add_text(root, NSLOCTEXT("Block", "Empty", "획득한 블록이 없습니다."), 22, FLinearColor(0.65f, 0.60f, 0.51f), FVector2D(LIST_LEFT, 190.0), FVector2D(WIDTH - LIST_LEFT, 36.0));
		empty_text_->SetJustification(ETextJustify::Center);

		// 사용할지 묻는 창은 블록 목록을 덮고, 판은 놓을 자리가 보이도록 덮지 않는다.
		confirm_panel_ = WidgetTree->ConstructWidget<UCanvasPanel>();
		confirm_panel_->SetVisibility(ESlateVisibility::Collapsed);
		UCanvasPanelSlot* confirm_slot = root->AddChildToCanvas(confirm_panel_);
		confirm_slot->SetPosition(FVector2D(LIST_LEFT, 0.0));
		confirm_slot->SetSize(FVector2D(WIDTH - LIST_LEFT, HEIGHT));
		UImage* dim = WidgetTree->ConstructWidget<UImage>();
		dim->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.022f, 0.016f, 0.010f, 0.92f), 4.0f, FLinearColor(1.0f, 0.61f, 0.12f), 2.0f));
		UCanvasPanelSlot* dim_slot = confirm_panel_->AddChildToCanvas(dim);
		dim_slot->SetSize(FVector2D(WIDTH - LIST_LEFT, HEIGHT));
		UTextBlock* question = add_text(confirm_panel_, NSLOCTEXT("Block", "Confirm", "사용하시겠습니까?"), 32, TEXT_COLOR, FVector2D(0.0, 130.0), FVector2D(WIDTH - LIST_LEFT, 50.0));
		question->SetJustification(ETextJustify::Center);
		FButtonStyle style;
		style.SetNormal(MakeButtonBrush(TEXT("/Game/Thats_Mine/UI/Shared/MI_ButtonWoodPlain.MI_ButtonWoodPlain")));
		style.SetHovered(MakeButtonBrush(TEXT("/Game/Thats_Mine/UI/Shared/MI_ButtonWoodHoverOrange.MI_ButtonWoodHoverOrange")));
		style.SetPressed(MakeButtonBrush(TEXT("/Game/Thats_Mine/UI/Shared/MI_ButtonWoodPressedOrange.MI_ButtonWoodPressedOrange")));
		style.SetNormalForeground(TEXT_COLOR);
		style.SetHoveredForeground(FLinearColor::White);
		style.SetPressedForeground(FLinearColor(1.0f, 0.84f, 0.45f));
		const FText button_names[] = { NSLOCTEXT("Block", "Use", "사용"), NSLOCTEXT("Block", "Cancel", "취소") };
		for (int32 index = 0; index < UE_ARRAY_COUNT(button_names); ++index)
		{
			UButton* button = WidgetTree->ConstructWidget<UButton>();
			button->SetStyle(style);
			UTextBlock* label = WidgetTree->ConstructWidget<UTextBlock>();
			label->SetText(button_names[index]);
			label->SetFont(GetGameFont(24));
			label->SetColorAndOpacity(FSlateColor::UseForeground());
			Cast<UButtonSlot>(button->AddChild(label))->SetPadding(FMargin(4.0f, 0.5f, 4.0f, 3.5f));
			UCanvasPanelSlot* button_slot = confirm_panel_->AddChildToCanvas(button);
			button_slot->SetPosition(FVector2D(15.0 + index * 170.0, 220.0));
			button_slot->SetSize(FVector2D(150.0, 52.0));
			if (index == 0)
			{
				button->OnClicked.AddDynamic(this, &UBlockBoardWidget::ConfirmUse);
			}
			else
			{
				button->OnClicked.AddDynamic(this, &UBlockBoardWidget::CancelUse);
			}
		}
	}
	return Super::RebuildWidget();
}

void UBlockBoardWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UBlockSubsystem* blocks = GetWorld()->GetSubsystem<UBlockSubsystem>())
	{
		blocks->on_blocks_changed_.AddUObject(this, &UBlockBoardWidget::RefreshBlocks);
	}
	if (UHintSubsystem* hints = GetWorld()->GetSubsystem<UHintSubsystem>())
	{
		hints->on_exclusion_mask_changed_.AddUObject(this, &UBlockBoardWidget::RefreshExclusion);
	}
	RefreshBlocks();
	RefreshExclusion();
}

void UBlockBoardWidget::NativeDestruct()
{
	if (UWorld* world = GetWorld())
	{
		if (UBlockSubsystem* blocks = world->GetSubsystem<UBlockSubsystem>())
		{
			blocks->on_blocks_changed_.RemoveAll(this);
		}
		if (UHintSubsystem* hints = world->GetSubsystem<UHintSubsystem>())
		{
			hints->on_exclusion_mask_changed_.RemoveAll(this);
		}
	}
	CancelDrag();
	CancelUse();
	Super::NativeDestruct();
}

void UBlockBoardWidget::RefreshBlocks()
{
	const UBlockSubsystem* blocks = GetWorld()->GetSubsystem<UBlockSubsystem>();
	usable_blocks_ = blocks ? blocks->GetUsableBlocks() : TArray<int32>();
	usable_block_counts_.Reset();
	int32 total_count = 0;
	for (const int32 piece : usable_blocks_)
	{
		total_count += usable_block_counts_.Add_GetRef(blocks->GetBlockCount(piece));
	}
	block_count_->SetText(FText::Format(NSLOCTEXT("Block", "Count", "획득한 블록 · {0}개"), FText::AsNumber(total_count)));
	empty_text_->SetVisibility(usable_blocks_.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (drag_.piece != INDEX_NONE && !usable_blocks_.Contains(drag_.piece))
	{
		CancelDrag();
	}
	if (pending_.piece != INDEX_NONE && !usable_blocks_.Contains(pending_.piece))
	{
		CancelUse();
	}
}

// 힌트 서브시스템의 배제 텍스처와 배제 판을 받아 둔다.
void UBlockBoardWidget::RefreshExclusion()
{
	const UHintSubsystem* hints = GetWorld()->GetSubsystem<UHintSubsystem>();
	exclusion_board_ = hints ? hints->GetExclusionBoard() : 0;
	exclusion_texture_ = hints ? hints->GetExclusionTexture() : nullptr;
	// 사용할지 묻는 중에 그 자리가 배제되면 묻기를 그만둔다.
	if (pending_.piece != INDEX_NONE && !CanPlace(pending_))
	{
		CancelUse();
	}
}

FVector2D UBlockBoardWidget::GetSlotPosition(int32 index) const
{
	return FVector2D(LIST_LEFT + (index % SLOT_COLUMNS) * SLOT_STEP, LIST_TOP + (index / SLOT_COLUMNS) * SLOT_STEP);
}

bool UBlockBoardWidget::IsOverBoard(const FVector2D& position) const
{
	return position.X >= 0.0 && position.X < BOARD_SIZE && position.Y >= 0.0 && position.Y < BOARD_SIZE;
}

bool UBlockBoardWidget::CanPlace(const FBlockPlacement& placement) const
{
	if (placement.piece == INDEX_NONE || placement.column < 0 || placement.row < 0)
	{
		return false;
	}
	const uint64 cells = GetBlockCells(placement.piece, placement.rotation, placement.column, placement.row);
	return cells != 0 && (cells & exclusion_board_) == 0;
}

UBlockBoardWidget::FBlockPlacement UBlockBoardWidget::GetSnappedPlacement() const
{
	const FIntPoint size = GetPieceSize(RotateBlockPiece(BLOCK_PIECES[drag_.piece], drag_.rotation));
	constexpr double cell_size = BOARD_SIZE / BLOCK_GRID_SIZE;
	FBlockPlacement placement = drag_;
	placement.column = FMath::FloorToInt32(cursor_.X / cell_size - size.X * 0.5 + 0.5);
	placement.row = FMath::FloorToInt32(cursor_.Y / cell_size - size.Y * 0.5 + 0.5);
	return placement;
}

int32 UBlockBoardWidget::NativePaint(const FPaintArgs& args, const FGeometry& geometry, const FSlateRect& culling_rect, FSlateWindowElementList& draw_elements, int32 layer_id, const FWidgetStyle& widget_style, bool is_parent_enabled) const
{
	int32 layer = layer_id;
	const FLinearColor tint = widget_style.GetColorAndOpacityTint();
	static const FSlateRoundedBoxBrush slot_brush(FLinearColor(0.022f, 0.016f, 0.010f, 0.72f), 4.0f, FLinearColor(0.18f, 0.12f, 0.075f), 1.5f);
	static const FSlateRoundedBoxBrush cell_brush(FLinearColor::White, 3.0f);
	static const FSlateRoundedBoxBrush frame_brush(FLinearColor::Transparent, 2.0f, FLinearColor(0.18f, 0.12f, 0.075f), 3.0f);
	const FSlateBrush* white_brush = FCoreStyle::Get().GetBrush("WhiteBrush");
	// 그리기 색에 붓 자체의 색(둥근 상자의 채움색)을 곱한다.
	const auto draw_box = [&](const FSlateBrush* brush, const FVector2D& offset, const FVector2D& box_size, const FLinearColor& color)
	{
		FSlateDrawElement::MakeBox(draw_elements, layer, geometry.ToPaintGeometry(FVector2f(box_size), FSlateLayoutTransform(FVector2f(offset))), brush, ESlateDrawEffect::None, color * brush->GetTint(widget_style) * tint);
	};
	// 블록 한 칸마다 틈을 두고 둥근 칸을 그린다.
	const auto draw_piece = [&](int32 piece, int32 rotation, const FVector2D& origin, double cell_size, const FLinearColor& color)
	{
		const BlockPiece rotated = RotateBlockPiece(BLOCK_PIECES[piece], rotation);
		const double gap = FMath::Max(1.0, cell_size * 0.06);
		for (int32 cell = 0; cell < rotated.cell_count; ++cell)
		{
			draw_box(&cell_brush, origin + FVector2D(rotated.cells[cell][0], rotated.cells[cell][1]) * cell_size + FVector2D(gap), FVector2D(cell_size - gap * 2.0), color);
		}
	};
	constexpr double cell_size = BOARD_SIZE / BLOCK_GRID_SIZE;

	// 판: 지도 사진, 배제된 곳 그늘, 배제 판 칸 그늘, 격자
	FSlateBrush map_brush;
	map_brush.SetResourceObject(map_texture_);
	draw_box(&map_brush, FVector2D::ZeroVector, FVector2D(BOARD_SIZE), FLinearColor::White);
	++layer;
	if (exclusion_texture_)
	{
		FSlateBrush exclusion_brush;
		exclusion_brush.SetResourceObject(exclusion_texture_);
		draw_box(&exclusion_brush, FVector2D::ZeroVector, FVector2D(BOARD_SIZE), FLinearColor(1.0f, 1.0f, 1.0f, EXCLUSION_SHADE_OPACITY));
		++layer;
	}
	for (int32 index = 0; index < BLOCK_GRID_SIZE * BLOCK_GRID_SIZE; ++index)
	{
		if ((exclusion_board_ >> index & 1) != 0)
		{
			draw_box(white_brush, FVector2D(index % BLOCK_GRID_SIZE, index / BLOCK_GRID_SIZE) * cell_size, FVector2D(cell_size), FLinearColor(0.0f, 0.0f, 0.0f, BOARD_SHADE_OPACITY));
		}
	}
	++layer;
	for (int32 line = 0; line <= BLOCK_GRID_SIZE; ++line)
	{
		const float position = static_cast<float>(line * cell_size);
		const FLinearColor line_color(0.96f, 0.90f, 0.78f, 0.55f);
		FSlateDrawElement::MakeLines(draw_elements, layer, geometry.ToPaintGeometry(), { FVector2f(position, 0.0f), FVector2f(position, static_cast<float>(BOARD_SIZE)) }, ESlateDrawEffect::None, line_color * tint, true, 1.0f);
		FSlateDrawElement::MakeLines(draw_elements, layer, geometry.ToPaintGeometry(), { FVector2f(0.0f, position), FVector2f(static_cast<float>(BOARD_SIZE), position) }, ESlateDrawEffect::None, line_color * tint, true, 1.0f);
	}
	++layer;
	draw_box(&frame_brush, FVector2D(-2.0), FVector2D(BOARD_SIZE + 4.0), FLinearColor::White);
	++layer;

	// 블록 목록: 칸마다 돌리지 않은 모양을 가운데에 그린다. 끄는 중인 블록은 흐리게 둔다.
	// 직접 그린 것은 자식 위젯 위에 그려지므로, 사용할지 묻는 창이 목록을 덮는 동안에는 목록을 그리지 않는다.
	const int32 slot_count = pending_.piece == INDEX_NONE ? usable_blocks_.Num() : 0;
	for (int32 index = 0; index < slot_count; ++index)
	{
		draw_box(&slot_brush, GetSlotPosition(index), FVector2D(SLOT_SIZE), FLinearColor::White);
	}
	++layer;
	for (int32 index = 0; index < slot_count; ++index)
	{
		const FIntPoint size = GetPieceSize(BLOCK_PIECES[usable_blocks_[index]]);
		const FVector2D origin = GetSlotPosition(index) + (FVector2D(SLOT_SIZE) - FVector2D(size) * SLOT_CELL_SIZE) * 0.5;
		draw_piece(usable_blocks_[index], 0, origin, SLOT_CELL_SIZE, usable_blocks_[index] == drag_.piece ? PIECE_COLOR * FLinearColor(1.0f, 1.0f, 1.0f, 0.3f) : PIECE_COLOR);
	}
	++layer;
	// 같은 블록이 2개 이상이면 칸 오른쪽 아래에 개수를 적는다.
	const FSlateFontInfo count_font = GetGameFont(SLOT_COUNT_FONT_SIZE);
	for (int32 index = 0; index < slot_count; ++index)
	{
		if (usable_block_counts_[index] < 2)
		{
			continue;
		}
		const FString count_text = FString::Printf(TEXT("×%d"), usable_block_counts_[index]);
		const FVector2D text_size = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(count_text, count_font);
		const FVector2D text_position = GetSlotPosition(index) + FVector2D(SLOT_SIZE) - text_size - FVector2D(5.0, 3.0);
		FSlateDrawElement::MakeText(draw_elements, layer, geometry.ToPaintGeometry(FVector2f(text_size), FSlateLayoutTransform(FVector2f(text_position))), count_text, count_font, ESlateDrawEffect::None, FLinearColor(1.0f, 0.94f, 0.79f) * tint);
	}
	++layer;

	// 사용할지 묻는 자리와 끄는 블록
	if (pending_.piece != INDEX_NONE)
	{
		draw_piece(pending_.piece, pending_.rotation, FVector2D(pending_.column, pending_.row) * cell_size, cell_size, VALID_COLOR);
	}
	// 판 위에서는 칸에 맞춰 붙이고, 판 밖에서는 가운데가 커서에 오게 그린다.
	if (drag_.piece != INDEX_NONE && IsOverBoard(cursor_))
	{
		const FBlockPlacement placement = GetSnappedPlacement();
		draw_piece(placement.piece, placement.rotation, FVector2D(placement.column, placement.row) * cell_size, cell_size, CanPlace(placement) ? VALID_COLOR : INVALID_COLOR);
	}
	else if (drag_.piece != INDEX_NONE)
	{
		const FIntPoint size = GetPieceSize(RotateBlockPiece(BLOCK_PIECES[drag_.piece], drag_.rotation));
		draw_piece(drag_.piece, drag_.rotation, cursor_ - FVector2D(size) * cell_size * 0.5, cell_size, PIECE_COLOR * FLinearColor(1.0f, 1.0f, 1.0f, 0.75f));
	}
	++layer;
	return Super::NativePaint(args, geometry, culling_rect, draw_elements, layer, widget_style, is_parent_enabled);
}

FReply UBlockBoardWidget::NativeOnMouseButtonDown(const FGeometry& geometry, const FPointerEvent& event)
{
	const FVector2D position = geometry.AbsoluteToLocal(event.GetScreenSpacePosition());
	if (pending_.piece != INDEX_NONE)
	{
		return FReply::Handled();
	}
	if (drag_.piece != INDEX_NONE)
	{
		if (event.GetEffectingButton() == EKeys::RightMouseButton)
		{
			RotateDrag();
		}
		return FReply::Handled();
	}
	if (event.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}
	for (int32 index = 0; index < usable_blocks_.Num(); ++index)
	{
		const FVector2D slot_position = GetSlotPosition(index);
		if (position.X >= slot_position.X && position.X < slot_position.X + SLOT_SIZE && position.Y >= slot_position.Y && position.Y < slot_position.Y + SLOT_SIZE)
		{
			drag_ = FBlockPlacement();
			drag_.piece = usable_blocks_[index];
			cursor_ = position;
			return FReply::Handled().CaptureMouse(TakeWidget()).SetUserFocus(TakeWidget());
		}
	}
	return FReply::Unhandled();
}

// 끄던 블록을 놓을 수 있는 자리에서 떼면 사용할지 묻고, 아니면 끌기를 그만둔다.
FReply UBlockBoardWidget::NativeOnMouseButtonUp(const FGeometry& geometry, const FPointerEvent& event)
{
	if (drag_.piece == INDEX_NONE)
	{
		return FReply::Unhandled();
	}
	if (event.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Handled();
	}
	cursor_ = geometry.AbsoluteToLocal(event.GetScreenSpacePosition());
	const FBlockPlacement placement = GetSnappedPlacement();
	if (IsOverBoard(cursor_) && CanPlace(placement))
	{
		pending_ = placement;
		confirm_panel_->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	drag_ = FBlockPlacement();
	return FReply::Handled().ReleaseMouseCapture();
}

FReply UBlockBoardWidget::NativeOnMouseMove(const FGeometry& geometry, const FPointerEvent& event)
{
	if (drag_.piece == INDEX_NONE)
	{
		return FReply::Unhandled();
	}
	cursor_ = geometry.AbsoluteToLocal(event.GetScreenSpacePosition());
	return FReply::Handled();
}

FReply UBlockBoardWidget::NativeOnMouseWheel(const FGeometry& geometry, const FPointerEvent& event)
{
	if (drag_.piece == INDEX_NONE)
	{
		return FReply::Unhandled();
	}
	RotateDrag();
	return FReply::Handled();
}

FReply UBlockBoardWidget::NativeOnKeyDown(const FGeometry& geometry, const FKeyEvent& event)
{
	if (drag_.piece == INDEX_NONE || event.GetKey() != EKeys::R)
	{
		return FReply::Unhandled();
	}
	RotateDrag();
	return FReply::Handled();
}

void UBlockBoardWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& event)
{
	CancelDrag();
	Super::NativeOnMouseCaptureLost(event);
}

void UBlockBoardWidget::RotateDrag()
{
	drag_.rotation = (drag_.rotation + 1) % BLOCK_ROTATION_COUNT;
}

void UBlockBoardWidget::CancelDrag()
{
	drag_ = FBlockPlacement();
}

void UBlockBoardWidget::ConfirmUse()
{
	if (pending_.piece != INDEX_NONE)
	{
		if (const UBlockSubsystem* blocks = GetWorld()->GetSubsystem<UBlockSubsystem>())
		{
			blocks->RequestUse(pending_.piece, pending_.rotation, pending_.column, pending_.row);
		}
	}
	CancelUse();
}

void UBlockBoardWidget::CancelUse()
{
	pending_ = FBlockPlacement();
	if (confirm_panel_)
	{
		confirm_panel_->SetVisibility(ESlateVisibility::Collapsed);
	}
}
