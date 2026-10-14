#include "HelpWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Settings/KeyBindingSubsystem.h"
#include "Styling/CoreStyle.h"

UHelpWidget::UHelpWidget(const FObjectInitializer& object_initializer)
	: Super(object_initializer)
{
	SetIsFocusable(true);
	frame_texture_ = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Thats_Mine/UI/Inventory/T_InventoryFrame.T_InventoryFrame")));
}

// 인벤토리와 같이 1200×900 틀을 화면에 맞춰 줄이고, 그 안에 제목과 설명 칸을 둔다.
TSharedRef<SWidget> UHelpWidget::RebuildWidget()
{
	if (!WidgetTree->RootWidget)
	{
		UCanvasPanel* root = WidgetTree->ConstructWidget<UCanvasPanel>();
		WidgetTree->RootWidget = root;
		UImage* dim = WidgetTree->ConstructWidget<UImage>();
		dim->SetBrush(FSlateColorBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.28f)));
		UCanvasPanelSlot* dim_slot = root->AddChildToCanvas(dim);
		dim_slot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		dim_slot->SetOffsets(FMargin(0.0f));

		UScaleBox* scale = WidgetTree->ConstructWidget<UScaleBox>();
		scale->SetStretch(EStretch::ScaleToFit);
		scale->SetStretchDirection(EStretchDirection::DownOnly);
		UCanvasPanelSlot* scale_slot = root->AddChildToCanvas(scale);
		scale_slot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		scale_slot->SetOffsets(FMargin(24.0f));
		USizeBox* size = WidgetTree->ConstructWidget<USizeBox>();
		size->SetWidthOverride(1200.0f);
		size->SetHeightOverride(900.0f);
		UScaleBoxSlot* size_slot = Cast<UScaleBoxSlot>(scale->AddChild(size));
		size_slot->SetHorizontalAlignment(HAlign_Center);
		size_slot->SetVerticalAlignment(VAlign_Center);
		UCanvasPanel* panel = WidgetTree->ConstructWidget<UCanvasPanel>();
		size->AddChild(panel);
		UImage* frame = WidgetTree->ConstructWidget<UImage>();
		frame->SetBrushFromTexture(frame_texture_.LoadSynchronous());
		frame->SetVisibility(ESlateVisibility::HitTestInvisible);
		panel->AddChildToCanvas(frame)->SetSize(FVector2D(1200.0, 900.0));
		UTextBlock* title = WidgetTree->ConstructWidget<UTextBlock>();
		title->SetText(NSLOCTEXT("Help", "Title", "도움말"));
		title->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 38));
		title->SetColorAndOpacity(FLinearColor(0.96f, 0.90f, 0.78f));
		UCanvasPanelSlot* title_slot = panel->AddChildToCanvas(title);
		title_slot->SetPosition(FVector2D(100.0, 43.0));
		title_slot->SetSize(FVector2D(700.0, 56.0));
		content_ = WidgetTree->ConstructWidget<UScrollBox>();
		content_->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
		content_->SetScrollbarThickness(FVector2D(10.0, 10.0));
		UCanvasPanelSlot* content_slot = panel->AddChildToCanvas(content_);
		content_slot->SetPosition(FVector2D(100.0, 125.0));
		content_slot->SetSize(FVector2D(1000.0, 690.0));
	}
	return Super::RebuildWidget();
}

void UHelpWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshContent();
}

void UHelpWidget::AddHeading(const FText& text)
{
	UTextBlock* heading = WidgetTree->ConstructWidget<UTextBlock>();
	heading->SetText(text);
	heading->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 26));
	heading->SetColorAndOpacity(FLinearColor(1.0f, 0.75f, 0.30f));
	Cast<UScrollBoxSlot>(content_->AddChild(heading))->SetPadding(FMargin(0.0f, 14.0f, 0.0f, 6.0f));
}

void UHelpWidget::AddLine(const FText& text)
{
	UTextBlock* line = WidgetTree->ConstructWidget<UTextBlock>();
	line->SetText(FText::Format(NSLOCTEXT("Help", "Bullet", "·  {0}"), text));
	line->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 20));
	line->SetColorAndOpacity(FLinearColor(0.96f, 0.90f, 0.78f));
	line->SetAutoWrapText(true);
	Cast<UScrollBoxSlot>(content_->AddChild(line))->SetPadding(FMargin(12.0f, 3.0f, 16.0f, 3.0f));
}

void UHelpWidget::RefreshContent()
{
	const UKeyBindingSubsystem* bindings = GetGameInstance()->GetSubsystem<UKeyBindingSubsystem>();
	const auto key = [bindings](EKeyAction action) { return bindings->GetKey(action).GetDisplayName(); };
	content_->ClearChildren();

	AddHeading(NSLOCTEXT("Help", "ControlsHeading", "조작 키"));
	AddLine(FText::Format(NSLOCTEXT("Help", "Move", "이동 {0} {1} {2} {3}, 점프 {4}"), key(EKeyAction::MOVE_FORWARD), key(EKeyAction::MOVE_LEFT), key(EKeyAction::MOVE_BACKWARD), key(EKeyAction::MOVE_RIGHT), key(EKeyAction::JUMP)));
	AddLine(NSLOCTEXT("Help", "Mouse", "왼쪽 클릭: 곡괭이 휘두르기·총 쏘기·돋보기 탐색, 오른쪽 클릭: 저격총 조준"));
	AddLine(FText::Format(NSLOCTEXT("Help", "Actions", "{0} 상호작용(상자 열기·두루마리 줍기), {1} 줍기, {2} 재장전"), key(EKeyAction::INTERACT), key(EKeyAction::PICKUP), key(EKeyAction::RELOAD)));
	AddLine(FText::Format(NSLOCTEXT("Help", "Weapons", "{0} 저격총, {1} 권총, {2} 곡괭이, {3} 돋보기(돋보기를 든 채 다시 누르면 기본·넓은 탐지 돋보기를 바꿈)"), key(EKeyAction::WEAPON_SNIPER), key(EKeyAction::WEAPON_PISTOL), key(EKeyAction::WEAPON_PICKAXE), key(EKeyAction::WEAPON_MAGNIFIER)));
	AddLine(FText::Format(NSLOCTEXT("Help", "Windows", "{0} 인벤토리, {1} 지도, {2} 도움말, ESC 메뉴(설정·나가기)"), key(EKeyAction::INVENTORY), key(EKeyAction::MAP), key(EKeyAction::HELP)));

	AddHeading(NSLOCTEXT("Help", "GoalHeading", "게임 목표와 진행"));
	AddLine(NSLOCTEXT("Help", "Goal", "맵 어딘가에 묻힌 보물을 가장 먼저 곡괭이로 파내면 승리합니다."));
	AddLine(NSLOCTEXT("Help", "TimeLimit", "제한시간은 2시간이고 남은 시간은 화면 오른쪽 위에 보입니다. 아무도 찾지 못하면 보물 위치가 공개되고 방으로 돌아갑니다."));
	AddLine(NSLOCTEXT("Help", "PublicHint", "일정 시간마다 모든 플레이어의 지도에 보물이 없는 같은 지역 하나가 공개됩니다."));
	AddLine(NSLOCTEXT("Help", "Pickaxe", "곡괭이는 최대 3번까지 휘두를 수 있고, 쓴 횟수는 시간이 지나면 다시 찹니다. 곡괭이를 들면 보물을 파낼 수 있는 범위가 발밑에 원으로 보입니다."));

	AddHeading(NSLOCTEXT("Help", "HintHeading", "힌트와 상자"));
	AddLine(NSLOCTEXT("Help", "Boxes", "땅 위의 나무 상자와 나무 위·공중의 선물 상자를 열면 힌트, 탄약, 아이템을 얻습니다. 선물 상자는 총으로 쏴서도 열 수 있습니다."));
	AddLine(NSLOCTEXT("Help", "Grades", "힌트는 S·A·B·C 등급이 있고, 높은 등급일수록 보물이 있을 곳을 많이 좁혀 줍니다. 주변 물체 개수 힌트는 S 10m, A 20m, B 30m, C 50m 안을 셉니다."));
	AddLine(NSLOCTEXT("Help", "Distance", "힌트와 돋보기, 핑의 거리는 높이까지 포함한 거리입니다."));
	AddLine(NSLOCTEXT("Help", "Exclusion", "돋보기 탐색 결과, 블록 사용 결과, 지역 공개로 보물이 없는 영역이 지도에 표시됩니다."));
	AddLine(NSLOCTEXT("Help", "Drop", "기절하면 가진 힌트 하나를 두루마리로 떨어뜨립니다. 떨어진 두루마리는 누구나 주울 수 있습니다."));

	AddHeading(NSLOCTEXT("Help", "CombatHeading", "무기·기절·아이템"));
	AddLine(NSLOCTEXT("Help", "Stun", "저격총·권총으로 다른 플레이어를 맞히면 기절 게이지가 오르고, 가득 차면 잠시 기절합니다. 기절이 끝나면 한동안 보호됩니다."));
	AddLine(NSLOCTEXT("Help", "Ammo", "탄약은 상자에서 얻습니다."));
	AddLine(NSLOCTEXT("Help", "Magnifier", "기본 돋보기는 언제나 가지고 있고, 쓴 자리 주변에 보물이 있는지 알려 줍니다. 한 번 쓰면 재사용 대기시간이 지나야 다시 쓸 수 있습니다."));
	AddLine(NSLOCTEXT("Help", "WideMagnifier", "넓은 탐지 돋보기는 상자에서 얻는 1회용 아이템으로, 훨씬 넓은 범위를 한 번 탐지합니다."));
	AddLine(NSLOCTEXT("Help", "Items", "S·A 등급 아이템은 곡괭이 충전, 기절 게이지, 보호 시간, 돋보기 범위·대기시간을 좋게 하고 게임 내내 유지됩니다."));
	content_->ScrollToStart();
}

FReply UHelpWidget::NativeOnPreviewKeyDown(const FGeometry& geometry, const FKeyEvent& event)
{
	const UKeyBindingSubsystem* bindings = GetGameInstance()->GetSubsystem<UKeyBindingSubsystem>();
	if (event.GetKey() == bindings->GetKey(EKeyAction::HELP) || event.GetKey() == EKeys::Escape)
	{
		if (!event.IsRepeat()) { on_close_requested_.ExecuteIfBound(); }
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(geometry, event);
}

FReply UHelpWidget::NativeOnKeyDown(const FGeometry& geometry, const FKeyEvent& event)
{
	// 이동과 점프는 게임 입력으로 넘기고, 나머지 동작은 도움말에서 소비한다.
	const UKeyBindingSubsystem* bindings = GetGameInstance()->GetSubsystem<UKeyBindingSubsystem>();
	for (const EKeyAction action : { EKeyAction::MOVE_FORWARD, EKeyAction::MOVE_BACKWARD, EKeyAction::MOVE_LEFT, EKeyAction::MOVE_RIGHT, EKeyAction::JUMP })
	{
		if (event.GetKey() == bindings->GetKey(action))
		{
			return FReply::Unhandled();
		}
	}
	return FReply::Handled();
}

FReply UHelpWidget::NativeOnMouseButtonDown(const FGeometry& geometry, const FPointerEvent& event)
{
	return FReply::Handled().SetUserFocus(TakeWidget());
}
