#include "MagnifierCountWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/CapsuleComponent.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Interaction/HintObjectSubsystem.h"
#include "Interaction/HintSubsystem.h"
#include "block_pieces.h"
#include "Player/PlayerCharacter.h"
#include "Styling/CoreStyle.h"
#include "UI/game_font.h"

TSharedRef<SWidget> UMagnifierCountWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		UCanvasPanel* root = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = root;
		list_box_ = WidgetTree->ConstructWidget<UVerticalBox>();
		UCanvasPanelSlot* box_slot = root->AddChildToCanvas(list_box_);
		box_slot->SetAnchors(FAnchors(1.0f, 0.5f));
		box_slot->SetAlignment(FVector2D(1.0, 0.5));
		box_slot->SetPosition(FVector2D(-SCREEN_MARGIN, 0.0));
		box_slot->SetAutoSize(true);
		// 좌표 표시와 같이 밝은 하늘이나 바닥 위에서도 읽히게 진한 외곽선을 두른다.
		FSlateFontInfo title_font = GetGameFont(TITLE_FONT_SIZE);
		title_font.OutlineSettings.OutlineSize = 2;
		title_font.OutlineSettings.OutlineColor = FLinearColor(0.03f, 0.02f, 0.01f, 0.9f);
		line_font_ = title_font;
		line_font_.Size = FONT_SIZE;
		UTextBlock* title_text = WidgetTree->ConstructWidget<UTextBlock>();
		title_text->SetFont(title_font);
		title_text->SetColorAndOpacity(FLinearColor(1.0f, 0.94f, 0.79f));
		title_text->SetJustification(ETextJustify::Right);
		title_text->SetText(FText::Format(NSLOCTEXT("Magnifier", "Title", "주변 {0}m"), FText::AsNumber(FMath::RoundToInt32(COUNT_RADIUS / 100.0f))));
		list_box_->AddChildToVerticalBox(title_text)->SetHorizontalAlignment(HAlign_Right);
		list_box_->SetVisibility(ESlateVisibility::Collapsed);
	}
	SetVisibility(ESlateVisibility::HitTestInvisible);
	return Super::RebuildWidget();
}

void UMagnifierCountWidget::NativeTick(const FGeometry& geometry, float delta_time)
{
	Super::NativeTick(geometry, delta_time);
	const APlayerCharacter* character = Cast<APlayerCharacter>(GetOwningPlayerPawn());
	if (!list_box_ || !character || character->GetCurrentWeapon() != EWeaponType::MAGNIFIER)
	{
		if (list_box_)
		{
			list_box_->SetVisibility(ESlateVisibility::Collapsed);
		}
		next_update_time_ = 0.0;
		return;
	}
	list_box_->SetVisibility(ESlateVisibility::HitTestInvisible);
	const double time = GetWorld()->GetTimeSeconds();
	if (time < next_update_time_)
	{
		return;
	}
	next_update_time_ = time + UPDATE_INTERVAL;
	// 서버 개수 힌트가 보물 바닥에서 세는 것처럼 발밑에서 센다.
	const UCapsuleComponent* capsule = character->GetCapsuleComponent();
	const FVector feet = capsule->GetComponentLocation() - FVector(0.0, 0.0, capsule->GetScaledCapsuleHalfHeight());
	const TArray<int32> counts = GetGameInstance()->GetSubsystem<UHintObjectSubsystem>()->CountKinds(feet, COUNT_RADIUS);
	const UHintSubsystem* hints = GetWorld()->GetSubsystem<UHintSubsystem>();
	const bool is_last_cell = FMath::CountBits64(hints->GetExclusionBoard()) == BLOCK_GRID_SIZE * BLOCK_GRID_SIZE - 1;
	// 이 종류·개수가 가진 단서 힌트와 같은지. 돋보기 반경과 같은 반경의 개수 힌트만 본다.
	const auto matches_clue = [character](int32 kind, int32 count)
	{
		return character->GetAcquiredHints().ContainsByPredicate([kind, count](const FInventoryHint& hint)
		{
			return hint.category == EHintCategory::CLUE && hint.radius < UE_ARRAY_COUNT(HINT_OBJECT_COUNT_RADII)
				&& HINT_OBJECT_COUNT_RADII[hint.radius] == COUNT_RADIUS && hint.object_kind == kind && hint.count == count;
		});
	};
	int32 line = 0;
	const auto add_line = [this, &line](const FText& text, const FLinearColor& color)
	{
		if (line == line_texts_.Num())
		{
			UTextBlock* line_text = WidgetTree->ConstructWidget<UTextBlock>();
			line_text->SetFont(line_font_);
			line_text->SetJustification(ETextJustify::Right);
			UVerticalBoxSlot* line_slot = list_box_->AddChildToVerticalBox(line_text);
			line_slot->SetHorizontalAlignment(HAlign_Right);
			line_slot->SetPadding(FMargin(0.0f, line == 0 ? 4.0f : 0.0f, 0.0f, 0.0f));
			line_texts_.Add(line_text);
		}
		line_texts_[line]->SetText(text);
		line_texts_[line]->SetColorAndOpacity(color);
		line_texts_[line]->SetVisibility(ESlateVisibility::HitTestInvisible);
		++line;
	};
	const FLinearColor normal_color(1.0f, 0.94f, 0.79f);
	const FLinearColor match_color(0.3f, 0.62f, 1.0f);
	for (int32 kind = 0; kind < counts.Num(); ++kind)
	{
		if (counts[kind] > 0)
		{
			const FText text = FText::FromString(FString::Printf(TEXT("%s  %d"), *hints->FindText(FString::Printf(TEXT("OBJECT_%d"), kind)), counts[kind]));
			add_line(text, is_last_cell && matches_clue(kind, counts[kind]) ? match_color : normal_color);
		}
	}
	if (line == 0)
	{
		add_line(NSLOCTEXT("Magnifier", "Empty", "힌트 물체 없음"), normal_color);
	}
	for (; line < line_texts_.Num(); ++line)
	{
		line_texts_[line]->SetVisibility(ESlateVisibility::Collapsed);
	}
}
