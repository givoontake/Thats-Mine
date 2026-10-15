#include "AwardWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Environment/AwardStage.h"
#include "Network/AwardSubsystem.h"
#include "Styling/CoreStyle.h"
#include "UI/game_font.h"

TSharedRef<SWidget> UAwardWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		UCanvasPanel* root = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = root;
		// 밝은 하늘·바닥 위에서도 읽히게 진한 외곽선을 두른다.
		const auto make_font = [](int32 size)
		{
			FSlateFontInfo font = GetGameFont(size);
			font.OutlineSettings.OutlineSize = 3;
			font.OutlineSettings.OutlineColor = FLinearColor(0.03f, 0.02f, 0.01f, 0.9f);
			return font;
		};
		title_text_ = WidgetTree->ConstructWidget<UTextBlock>();
		title_text_->SetFont(make_font(TITLE_FONT_SIZE));
		title_text_->SetColorAndOpacity(FLinearColor(1.0f, 0.83f, 0.25f));
		title_text_->SetJustification(ETextJustify::Center);
		title_slot_ = root->AddChildToCanvas(title_text_);
		title_slot_->SetAnchors(FAnchors(0.5f, 0.0f));
		title_slot_->SetAlignment(FVector2D(0.5, 0.0));
		title_slot_->SetAutoSize(true);
		// 순위 숫자: 1위 금색, 2위 은색, 3위 동색. 가운데를 시상대 앞면 가운데에 맞춘다.
		static const FLinearColor RANK_COLORS[AAwardStage::RANK_COUNT] = { FLinearColor(1.0f, 0.8f, 0.24f), FLinearColor(0.84f, 0.86f, 0.9f), FLinearColor(0.8f, 0.49f, 0.24f) };
		for (int32 rank = 0; rank < AAwardStage::RANK_COUNT; ++rank)
		{
			UTextBlock* rank_text = WidgetTree->ConstructWidget<UTextBlock>();
			rank_text->SetFont(make_font(RANK_FONT_SIZE));
			rank_text->SetColorAndOpacity(RANK_COLORS[rank]);
			rank_text->SetText(FText::AsNumber(rank + 1));
			rank_text->SetVisibility(ESlateVisibility::Collapsed);
			UCanvasPanelSlot* rank_slot = root->AddChildToCanvas(rank_text);
			rank_slot->SetAlignment(FVector2D(0.5, 0.5));
			rank_slot->SetAutoSize(true);
			rank_texts_.Add(rank_text);
			rank_slots_.Add(rank_slot);
		}
		for (int32 rank = 0; rank < AAwardStage::RANK_COUNT; ++rank)
		{
			// 이름표: 위에 닉네임, 아래에 횟수. 아래쪽 가운데를 머리 위 자리에 맞춘다.
			UVerticalBox* name_tag = WidgetTree->ConstructWidget<UVerticalBox>();
			name_tag->SetVisibility(ESlateVisibility::Collapsed);
			UTextBlock* name_text = WidgetTree->ConstructWidget<UTextBlock>();
			name_text->SetFont(make_font(NAME_FONT_SIZE));
			name_text->SetColorAndOpacity(FLinearColor(1.0f, 0.97f, 0.9f));
			name_text->SetJustification(ETextJustify::Center);
			name_tag->AddChildToVerticalBox(name_text)->SetHorizontalAlignment(HAlign_Center);
			UTextBlock* record_text = WidgetTree->ConstructWidget<UTextBlock>();
			record_text->SetFont(make_font(RECORD_FONT_SIZE));
			record_text->SetColorAndOpacity(FLinearColor(1.0f, 0.83f, 0.25f));
			record_text->SetJustification(ETextJustify::Center);
			name_tag->AddChildToVerticalBox(record_text)->SetHorizontalAlignment(HAlign_Center);
			UCanvasPanelSlot* name_slot = root->AddChildToCanvas(name_tag);
			name_slot->SetAlignment(FVector2D(0.5, 1.0));
			name_slot->SetAutoSize(true);
			name_texts_.Add(name_text);
			record_texts_.Add(record_text);
			name_tags_.Add(name_tag);
			name_slots_.Add(name_slot);
		}
		skip_button_ = WidgetTree->ConstructWidget<UButton>();
		skip_button_->SetBackgroundColor(FLinearColor(0.08f, 0.08f, 0.1f, 0.75f));
		skip_button_->OnClicked.AddDynamic(this, &UAwardWidget::HandleSkipClicked);
		UTextBlock* skip_text = WidgetTree->ConstructWidget<UTextBlock>();
		skip_text->SetFont(make_font(20));
		skip_text->SetText(NSLOCTEXT("Award", "Skip", "건너뛰기"));
		skip_button_->AddChild(skip_text);
		UCanvasPanelSlot* skip_slot = root->AddChildToCanvas(skip_button_);
		skip_slot->SetAnchors(FAnchors(1.0f, 1.0f));
		skip_slot->SetAlignment(FVector2D(1.0, 1.0));
		skip_slot->SetPosition(FVector2D(-SCREEN_MARGIN, -SCREEN_MARGIN));
		skip_slot->SetAutoSize(true);
	}
	return Super::RebuildWidget();
}

void UAwardWidget::ShowScene(AAwardStage* stage, const FText& title, const TArray<FString>& nicknames, const TArray<FText>& records)
{
	stage_ = stage;
	title_text_->SetText(title);
	title_start_time_ = GetWorld()->GetTimeSeconds();
	name_count_ = FMath::Min3(nicknames.Num(), records.Num(), name_tags_.Num());
	for (int32 rank = 0; rank < name_tags_.Num(); ++rank)
	{
		const bool is_shown = rank < name_count_;
		name_tags_[rank]->SetVisibility(is_shown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (is_shown)
		{
			name_texts_[rank]->SetText(FText::FromString(nicknames[rank]));
			record_texts_[rank]->SetText(records[rank]);
		}
	}
}

void UAwardWidget::NativeTick(const FGeometry& geometry, float delta_time)
{
	Super::NativeTick(geometry, delta_time);
	// 제목: 위에서 내려오며 끝에서 천천히 멈춘다.
	const float alpha = FMath::Clamp(static_cast<float>(GetWorld()->GetTimeSeconds() - title_start_time_) / TITLE_DROP_SECONDS, 0.0f, 1.0f);
	title_slot_->SetPosition(FVector2D(0.0, TITLE_TOP - TITLE_DROP_DISTANCE * (1.0f - FMath::InterpEaseOut(0.0f, 1.0f, alpha, 3.0f))));
	// 이름: 캐릭터 머리 위 자리를 화면에 옮겨 그 위에 둔다. 순위 숫자도 시상대 앞면 자리를 옮겨 둔다.
	APlayerController* controller = GetOwningPlayer();
	for (int32 rank = 0; rank < rank_texts_.Num() && stage_ && controller; ++rank)
	{
		FVector2D position;
		const bool is_on_screen = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(controller, stage_->GetRankLocation(rank), position, false);
		rank_texts_[rank]->SetVisibility(is_on_screen ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		rank_slots_[rank]->SetPosition(position);
	}
	for (int32 rank = 0; rank < name_count_ && stage_ && controller; ++rank)
	{
		FVector2D position;
		if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(controller, stage_->GetNameLocation(rank), position, false))
		{
			name_slots_[rank]->SetPosition(position);
		}
	}
}

void UAwardWidget::HandleSkipClicked()
{
	if (UAwardSubsystem* award = GetWorld()->GetSubsystem<UAwardSubsystem>())
	{
		award->Skip();
	}
}
