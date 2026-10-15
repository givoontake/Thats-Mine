#include "SettingsWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/EditableText.h"
#include "Components/ScrollBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameUserSettings.h"
#include "CycleOptionRow.h"
#include "Settings/FoliageShadowSubsystem.h"
#include "Settings/AudioSettingsSubsystem.h"
#include "Settings/FrameRateSettingsSubsystem.h"
#include "Player/ControlSettingsSubsystem.h"
#include "Settings/KeyBindingSubsystem.h"

static constexpr float SENSITIVITY_STEP = 0.1f;
static constexpr float VOLUME_STEP = 0.01f;
static constexpr float VOLUME_TEXT_SCALE = 100.0f;

// 그래픽 탭 항목. 화면의 세 단계는 엔진 품질 묶음의 1~3(중간·높음·최고)이다. 엔진 0은 그림자·풀이 통째로 없어져 쓰지 않는다.
// 풀은 데이지 힌트와 숨기·보기에 영향을 주므로 넣지 않는다.
struct GraphicsQualityOption
{
	const TCHAR* label;
	const TCHAR* description;
	int32 (UGameUserSettings::*get)() const;
	void (UGameUserSettings::*set)(int32);
};
static const GraphicsQualityOption GRAPHICS_QUALITY_OPTIONS[] = {
	{ TEXT("시야 거리"), TEXT("멀리 있는 물체가 보이는 거리와 먼 곳의 세밀함을 정합니다."), &UGameUserSettings::GetViewDistanceQuality, &UGameUserSettings::SetViewDistanceQuality },
	{ TEXT("안티앨리어싱"), TEXT("물체 가장자리의 계단 모양을 매끄럽게 다듬는 품질입니다."), &UGameUserSettings::GetAntiAliasingQuality, &UGameUserSettings::SetAntiAliasingQuality },
	{ TEXT("그림자"), TEXT("그림자의 선명도와 그림자가 보이는 거리입니다. 끔은 낮음에서 나무·풀 같은 폴리지 그림자까지 그리지 않습니다."), &UGameUserSettings::GetShadowQuality, &UGameUserSettings::SetShadowQuality },
	{ TEXT("전역 조명"), TEXT("빛이 주변에 튕겨 그늘을 밝히는 간접광 품질입니다."), &UGameUserSettings::GetGlobalIlluminationQuality, &UGameUserSettings::SetGlobalIlluminationQuality },
	{ TEXT("반사"), TEXT("물과 매끈한 표면에 비치는 반사의 품질입니다."), &UGameUserSettings::GetReflectionQuality, &UGameUserSettings::SetReflectionQuality },
	{ TEXT("후처리"), TEXT("빛 번짐, 움직임 흐림, 안개 같은 화면 효과의 품질입니다."), &UGameUserSettings::GetPostProcessingQuality, &UGameUserSettings::SetPostProcessingQuality },
	{ TEXT("텍스처"), TEXT("물체 표면 그림의 해상도입니다."), &UGameUserSettings::GetTextureQuality, &UGameUserSettings::SetTextureQuality },
	{ TEXT("효과"), TEXT("총구 화염, 기절 효과 같은 파티클 효과의 품질입니다."), &UGameUserSettings::GetVisualEffectQuality, &UGameUserSettings::SetVisualEffectQuality },
	{ TEXT("셰이딩"), TEXT("물체 표면의 빛과 재질을 계산하는 정밀도입니다."), &UGameUserSettings::GetShadingQuality, &UGameUserSettings::SetShadingQuality },
	{ TEXT("지형"), TEXT("땅 표면의 세밀함입니다."), &UGameUserSettings::GetLandscapeQuality, &UGameUserSettings::SetLandscapeQuality },
};
static constexpr int32 GRAPHICS_QUALITY_OPTION_COUNT = UE_ARRAY_COUNT(GRAPHICS_QUALITY_OPTIONS);
// 그림자 항목은 낮음 앞에 "끔"(폴리지 그림자 끔) 단계가 하나 더 있다.
static constexpr int32 SHADOW_OPTION = 2;
// 품질 단계 수와 렌더 해상도 단계마다의 화면 비율(%).
static constexpr int32 QUALITY_LEVEL_COUNT = 3;
static constexpr float RESOLUTION_SCALES[QUALITY_LEVEL_COUNT] = { 50.0f, 75.0f, 100.0f };
static constexpr int32 ENGINE_QUALITY_OFFSET = 1;
// 기본값 단계. 품질은 높음, 렌더 해상도는 100%다.
static constexpr int32 DEFAULT_QUALITY_LEVEL = QUALITY_LEVEL_COUNT - 1;
static constexpr int32 DEFAULT_RESOLUTION_LEVEL = QUALITY_LEVEL_COUNT - 1;
// 품질 항목 줄의 단계 번호에서 낮음 앞에 붙은 단계 수. 그림자 줄만 1이다.
static int32 GetLeadingSteps(int32 option)
{
	return option == SHADOW_OPTION ? 1 : 0;
}
static const TCHAR* DESCRIPTION_HINT = TEXT("항목에 마우스를 올리면 설명이 표시됩니다.");
// 조작 탭에서 감도 칸이 차지하는 높이와 키 줄 크기
static constexpr float SENSITIVITY_AREA_HEIGHT = 190.0f;
static constexpr float KEY_LABEL_WIDTH = 320.0f;
static constexpr float KEY_BUTTON_WIDTH = 220.0f;
static constexpr float KEY_BUTTON_HEIGHT = 46.0f;

void USettingsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	audio_tab_->OnClicked.AddDynamic(this, &USettingsWidget::SelectAudioTab);
	controls_tab_->OnClicked.AddDynamic(this, &USettingsWidget::SelectControlsTab);
	graphics_tab_->OnClicked.AddDynamic(this, &USettingsWidget::SelectGraphicsTab);
	defaults_button_->OnClicked.AddDynamic(this, &USettingsWidget::ResetToDefaults);
	apply_button_->OnClicked.AddDynamic(this, &USettingsWidget::ApplySettings);
	const UFrameRateSettingsSubsystem* frame_rate = GetGameInstance() ? GetGameInstance()->GetSubsystem<UFrameRateSettingsSubsystem>() : nullptr;
	TArray<FText> lobby_frame_names;
	for (int32 option = 0; option < UFrameRateSettingsSubsystem::LOBBY_OPTION_COUNT; ++option)
	{
		lobby_frame_names.Add(FText::AsNumber(frame_rate ? FMath::RoundToInt(frame_rate->GetLobbyLimit(option)) : 0));
	}
	TArray<FText> game_frame_names;
	for (int32 option = 0; option < UFrameRateSettingsSubsystem::GAME_OPTION_MONITOR; ++option)
	{
		game_frame_names.Add(FText::FromString(FString::Printf(TEXT("%d"), frame_rate ? FMath::RoundToInt(frame_rate->GetGameLimit(option)) : 0)));
	}
	game_frame_names.Add(FText::FromString(TEXT("모니터에 맞춤")));
	game_frame_names.Add(FText::FromString(TEXT("제한 없음")));
	lobby_frame_row_ = AddOptionRow(FText::FromString(TEXT("로비 프레임 제한")), lobby_frame_names, FText::FromString(TEXT("로그인·로비·방 화면의 초당 프레임 수 상한입니다.")));
	lobby_frame_row_->on_option_stepped_.BindUObject(this, &USettingsWidget::StepLobbyFrame);
	game_frame_row_ = AddOptionRow(FText::FromString(TEXT("게임 프레임 제한")), game_frame_names, FText::FromString(TEXT("게임 중 초당 프레임 수 상한입니다.")));
	game_frame_row_->on_option_stepped_.BindUObject(this, &USettingsWidget::StepGameFrame);
	TArray<FText> resolution_names;
	for (const float scale : RESOLUTION_SCALES)
	{
		resolution_names.Add(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(scale))));
	}
	resolution_row_ = AddOptionRow(FText::FromString(TEXT("렌더 해상도")), resolution_names, FText::FromString(TEXT("낮추면 성능은 향상되지만 화면이 흐릿해질 수 있습니다.")));
	resolution_row_->on_option_stepped_.BindUObject(this, &USettingsWidget::StepResolution);
	const TArray<FText> quality_names = { FText::FromString(TEXT("낮음")), FText::FromString(TEXT("중간")), FText::FromString(TEXT("높음")) };
	overall_quality_row_ = AddOptionRow(FText::FromString(TEXT("전체 품질")), quality_names, FText::FromString(TEXT("아래 품질 항목 10개를 한 번에 바꿉니다.")));
	overall_quality_row_->on_option_stepped_.BindUObject(this, &USettingsWidget::StepOverallQuality);
	for (int32 option = 0; option < GRAPHICS_QUALITY_OPTION_COUNT; ++option)
	{
		TArray<FText> row_names = quality_names;
		if (option == SHADOW_OPTION)
		{
			row_names.Insert(FText::FromString(TEXT("끔")), 0);
		}
		UCycleOptionRow* row = AddOptionRow(FText::FromString(GRAPHICS_QUALITY_OPTIONS[option].label), row_names, FText::FromString(GRAPHICS_QUALITY_OPTIONS[option].description));
		row->on_option_stepped_.BindUObject(this, &USettingsWidget::StepQuality, option);
		quality_rows_.Add(row);
	}
	for (USlider* slider : {master_volume_.Get(), music_volume_.Get(), effects_volume_.Get()})
	{
		slider->SetStepSize(VOLUME_STEP);
		slider->OnValueChanged.AddDynamic(this, &USettingsWidget::UpdateVolumeLabels);
	}
	for (UEditableText* text : {master_volume_text_.Get(), music_volume_text_.Get(), effects_volume_text_.Get(), mouse_sensitivity_text_.Get(), zoom_sensitivity_text_.Get()})
	{
		text->SetSelectAllTextWhenFocused(true);
		text->SetClearKeyboardFocusOnCommit(true);
	}
	for (UEditableText* text : {master_volume_text_.Get(), music_volume_text_.Get(), effects_volume_text_.Get()})
	{
		text->OnTextCommitted.AddDynamic(this, &USettingsWidget::CommitVolumeText);
	}
	for (UEditableText* text : {mouse_sensitivity_text_.Get(), zoom_sensitivity_text_.Get()})
	{
		text->OnTextCommitted.AddDynamic(this, &USettingsWidget::CommitSensitivityText);
	}
	UpdateVolumeLabels(0.0f);
	for (USlider* slider : {mouse_sensitivity_.Get(), zoom_sensitivity_.Get()})
	{
		slider->SetMinValue(UControlSettingsSubsystem::MIN_SENSITIVITY);
		slider->SetMaxValue(UControlSettingsSubsystem::MAX_SENSITIVITY);
		slider->SetStepSize(SENSITIVITY_STEP);
		slider->OnValueChanged.AddDynamic(this, &USettingsWidget::UpdateSensitivityLabels);
	}
	BuildKeyRows();
	SelectGraphicsTab();
}

void USettingsWidget::BuildKeyRows()
{
	UCanvasPanel* page = Cast<UCanvasPanel>(mouse_sensitivity_->GetParent());
	if (!page)
	{
		return;
	}
	UScrollBox* scroll = WidgetTree->ConstructWidget<UScrollBox>();
	scroll->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
	scroll->SetScrollbarThickness(FVector2D(8.0, 8.0));
	UCanvasPanel* sensitivity_area = WidgetTree->ConstructWidget<UCanvasPanel>();
	USizeBox* sensitivity_size = WidgetTree->ConstructWidget<USizeBox>();
	sensitivity_size->SetHeightOverride(SENSITIVITY_AREA_HEIGHT);
	sensitivity_size->AddChild(sensitivity_area);
	// 감도 줄은 배치를 그대로 지닌 채 스크롤 상자 맨 위 칸으로 옮긴다.
	for (UWidget* child : page->GetAllChildren())
	{
		const FAnchorData layout = CastChecked<UCanvasPanelSlot>(child->Slot)->GetLayout();
		child->RemoveFromParent();
		sensitivity_area->AddChildToCanvas(child)->SetLayout(layout);
	}
	UCanvasPanelSlot* scroll_slot = page->AddChildToCanvas(scroll);
	scroll_slot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
	scroll_slot->SetOffsets(FMargin(0.0f));
	scroll->AddChild(sensitivity_size);
	// 키 칸은 테두리·배경 없이 글자만 두고, 마우스를 올리면 탭 버튼과 같은 글자색으로 바꾼다.
	FButtonStyle key_style = tab_button_style_;
	key_style.Normal.DrawAs = ESlateBrushDrawType::NoDrawType;
	key_style.Hovered.DrawAs = ESlateBrushDrawType::NoDrawType;
	key_style.Pressed.DrawAs = ESlateBrushDrawType::NoDrawType;
	key_style.Disabled.DrawAs = ESlateBrushDrawType::NoDrawType;
	for (int32 action = 0; action < static_cast<int32>(EKeyAction::COUNT); ++action)
	{
		UHorizontalBox* row = WidgetTree->ConstructWidget<UHorizontalBox>();
		UTextBlock* label = WidgetTree->ConstructWidget<UTextBlock>();
		label->SetText(UKeyBindingSubsystem::GetActionName(static_cast<EKeyAction>(action)));
		label->SetFont(option_label_font_);
		USizeBox* label_size = WidgetTree->ConstructWidget<USizeBox>();
		label_size->SetWidthOverride(KEY_LABEL_WIDTH);
		label_size->AddChild(label);
		row->AddChildToHorizontalBox(label_size)->SetVerticalAlignment(VAlign_Center);
		UButton* button = WidgetTree->ConstructWidget<UButton>();
		button->SetStyle(key_style);
		button->OnClicked.AddDynamic(this, &USettingsWidget::StartKeyCapture);
		UTextBlock* key = WidgetTree->ConstructWidget<UTextBlock>();
		key->SetFont(option_level_font_);
		key->SetColorAndOpacity(FSlateColor::UseForeground());
		key->SetJustification(ETextJustify::Center);
		Cast<UButtonSlot>(button->AddChild(key))->SetPadding(FMargin(4.0f, 0.5f, 4.0f, 3.5f));
		USizeBox* button_size = WidgetTree->ConstructWidget<USizeBox>();
		button_size->SetWidthOverride(KEY_BUTTON_WIDTH);
		button_size->SetHeightOverride(KEY_BUTTON_HEIGHT);
		button_size->AddChild(button);
		row->AddChildToHorizontalBox(button_size)->SetVerticalAlignment(VAlign_Center);
		Cast<UScrollBoxSlot>(scroll->AddChild(row))->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 4.0f));
		key_buttons_.Add(button);
		key_texts_.Add(key);
	}
}

void USettingsWidget::StartKeyCapture()
{
	capturing_action_ = key_buttons_.IndexOfByPredicate([](const UButton* button) { return button->IsHovered(); });
	option_description_->SetText(capturing_action_ == INDEX_NONE ? FText::GetEmpty() : NSLOCTEXT("Settings", "KeyCapture", "바꿀 키를 누르세요. ESC를 누르면 취소합니다."));
	UpdateKeyTexts();
}

void USettingsWidget::UpdateKeyTexts()
{
	for (int32 action = 0; action < key_texts_.Num() && action < pending_keys_.Num(); ++action)
	{
		key_texts_[action]->SetText(action == capturing_action_ ? NSLOCTEXT("Settings", "PressKey", "키 입력") : pending_keys_[action].GetDisplayName());
		// 키 입력을 기다리는 칸은 누른 버튼 글자색으로 표시한다.
		key_texts_[action]->SetColorAndOpacity(action == capturing_action_ ? tab_button_style_.PressedForeground : FSlateColor::UseForeground());
	}
}

void USettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	LoadSavedSettings();
}

void USettingsWidget::LoadSavedSettings()
{
	pending_keys_.Reset();
	const UKeyBindingSubsystem* bindings = GetGameInstance()->GetSubsystem<UKeyBindingSubsystem>();
	for (int32 action = 0; action < static_cast<int32>(EKeyAction::COUNT); ++action)
	{
		pending_keys_.Add(bindings->GetKey(static_cast<EKeyAction>(action)));
	}
	capturing_action_ = INDEX_NONE;
	UpdateKeyTexts();
	if (const UControlSettingsSubsystem* settings = GetGameInstance() ? GetGameInstance()->GetSubsystem<UControlSettingsSubsystem>() : nullptr)
	{
		mouse_sensitivity_->SetValue(settings->GetMouseSensitivity());
		zoom_sensitivity_->SetValue(settings->GetZoomSensitivity());
	}
	UpdateSensitivityLabels(0.0f);
	if (const UAudioSettingsSubsystem* audio = GetGameInstance() ? GetGameInstance()->GetSubsystem<UAudioSettingsSubsystem>() : nullptr)
	{
		master_volume_->SetValue(audio->GetMasterVolume());
		music_volume_->SetValue(audio->GetMusicVolume());
		effects_volume_->SetValue(audio->GetEffectsVolume());
	}
	UpdateVolumeLabels(0.0f);
	UpdateGraphicsRows();
}

FReply USettingsWidget::NativeOnPreviewKeyDown(const FGeometry& geometry, const FKeyEvent& event)
{
	// 바꿀 키를 기다리는 중이면 키보드 키 하나를 받는다.
	if (capturing_action_ != INDEX_NONE)
	{
		const FKey key = event.GetKey();
		if (!key.IsMouseButton() && !key.IsGamepadKey())
		{
			CaptureKey(key);
		}
		return FReply::Handled();
	}
	if (event.GetKey() == EKeys::Escape && on_close_requested_.IsBound())
	{
		if (!event.IsRepeat()) { on_close_requested_.Execute(); }
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(geometry, event);
}

// 바꿀 키를 기다리는 중이면 왼쪽·오른쪽 버튼이 아닌 마우스 버튼(휠 클릭 등)도 받는다.
FReply USettingsWidget::NativeOnPreviewMouseButtonDown(const FGeometry& geometry, const FPointerEvent& event)
{
	const FKey key = event.GetEffectingButton();
	if (capturing_action_ != INDEX_NONE && key != EKeys::LeftMouseButton && key != EKeys::RightMouseButton)
	{
		CaptureKey(key);
		return FReply::Handled();
	}
	return Super::NativeOnPreviewMouseButtonDown(geometry, event);
}

void USettingsWidget::CaptureKey(const FKey& key)
{
	if (key != EKeys::Escape)
	{
		const int32 other = pending_keys_.IndexOfByKey(key);
		if (other != INDEX_NONE)
		{
			pending_keys_[other] = pending_keys_[capturing_action_];
		}
		pending_keys_[capturing_action_] = key;
	}
	capturing_action_ = INDEX_NONE;
	option_description_->SetText(FText::GetEmpty());
	UpdateKeyTexts();
}

void USettingsWidget::SelectAudioTab()
{
	SelectTab(audio_tab_);
}

void USettingsWidget::SelectControlsTab()
{
	SelectTab(controls_tab_);
}

void USettingsWidget::SelectGraphicsTab()
{
	SelectTab(graphics_tab_);
}

void USettingsWidget::SelectTab(UButton* button)
{
	int32 index = 0;
	option_description_->SetText(button == graphics_tab_ ? FText::FromString(DESCRIPTION_HINT) : FText::GetEmpty());
	for (UButton* tab : {graphics_tab_.Get(), audio_tab_.Get(), controls_tab_.Get()})
	{
		ApplyTabStyle(tab, tab_button_style_, selected_tab_brush_, tab == button);
		if (tab == button)
		{
			settings_pages_->SetActiveWidgetIndex(index);
		}
		++index;
	}
}

void USettingsWidget::ApplyTabStyle(UButton* button, const FButtonStyle& tab_style, const FSlateBrush& selected_brush, bool is_selected)
{
	FButtonStyle style = button->GetStyle();
	style.Normal = tab_style.Normal;
	style.Normal.ImageSize *= style.Hovered.ImageSize.Y / style.Normal.ImageSize.Y;
	style.NormalForeground = tab_style.NormalForeground;
	style.Hovered.SetResourceObject(tab_style.Hovered.GetResourceObject());
	style.HoveredForeground = tab_style.HoveredForeground;
	if (is_selected)
	{
		style.Normal.SetResourceObject(selected_brush.GetResourceObject());
		style.NormalForeground = style.PressedForeground;
		style.Hovered = style.Normal;
		style.HoveredForeground = style.PressedForeground;
	}
	button->SetStyle(style);
}

void USettingsWidget::UpdateVolumeLabels(float value)
{
	master_volume_text_->SetText(FText::AsNumber(FMath::RoundToInt(master_volume_->GetValue() * VOLUME_TEXT_SCALE)));
	music_volume_text_->SetText(FText::AsNumber(FMath::RoundToInt(music_volume_->GetValue() * VOLUME_TEXT_SCALE)));
	effects_volume_text_->SetText(FText::AsNumber(FMath::RoundToInt(effects_volume_->GetValue() * VOLUME_TEXT_SCALE)));
}

// 입력한 숫자를 정수로 반올림해 슬라이더에 넣는다. 숫자가 아니거나 입력을 취소하면 표시만 원래 값으로 되돌린다.
void USettingsWidget::CommitVolumeText(const FText& text, ETextCommit::Type commit_type)
{
	if (commit_type != ETextCommit::OnCleared)
	{
		for (const TPair<USlider*, UEditableText*>& volume : {TPair<USlider*, UEditableText*>(master_volume_, master_volume_text_), TPair<USlider*, UEditableText*>(music_volume_, music_volume_text_), TPair<USlider*, UEditableText*>(effects_volume_, effects_volume_text_)})
		{
			const FString input = volume.Value->GetText().ToString().TrimStartAndEnd();
			if (input.IsNumeric())
			{
				volume.Key->SetValue(FMath::Clamp(FMath::RoundToFloat(FCString::Atof(*input)), 0.0f, VOLUME_TEXT_SCALE) / VOLUME_TEXT_SCALE);
			}
		}
	}
	UpdateVolumeLabels(0.0f);
}

void USettingsWidget::UpdateSensitivityLabels(float value)
{
	FNumberFormattingOptions format;
	format.SetMinimumFractionalDigits(1);
	format.SetMaximumFractionalDigits(1);
	mouse_sensitivity_text_->SetText(FText::AsNumber(mouse_sensitivity_->GetValue(), &format));
	zoom_sensitivity_text_->SetText(FText::AsNumber(zoom_sensitivity_->GetValue(), &format));
}

// 입력한 숫자를 소수 첫째 자리로 반올림하고 범위 안으로 맞춰 슬라이더에 넣는다.
void USettingsWidget::CommitSensitivityText(const FText& text, ETextCommit::Type commit_type)
{
	if (commit_type != ETextCommit::OnCleared)
	{
		for (const TPair<USlider*, UEditableText*>& sensitivity : {TPair<USlider*, UEditableText*>(mouse_sensitivity_, mouse_sensitivity_text_), TPair<USlider*, UEditableText*>(zoom_sensitivity_, zoom_sensitivity_text_)})
		{
			const FString input = sensitivity.Value->GetText().ToString().TrimStartAndEnd();
			if (input.IsNumeric())
			{
				sensitivity.Key->SetValue(FMath::Clamp(FMath::RoundToFloat(FCString::Atof(*input) / SENSITIVITY_STEP) * SENSITIVITY_STEP, UControlSettingsSubsystem::MIN_SENSITIVITY, UControlSettingsSubsystem::MAX_SENSITIVITY));
			}
		}
	}
	UpdateSensitivityLabels(0.0f);
}

UCycleOptionRow* USettingsWidget::AddOptionRow(const FText& label, const TArray<FText>& option_names, const FText& description)
{
	UCycleOptionRow* row = CreateWidget<UCycleOptionRow>(this, UCycleOptionRow::StaticClass());
	row->Setup(label, option_names, option_label_font_, option_level_font_, tab_button_style_, selected_tab_brush_);
	row->on_hovered_.BindWeakLambda(this, [this, description]() { option_description_->SetText(description); });
	graphics_options_->AddChild(row);
	return row;
}

void USettingsWidget::UpdateGraphicsRows()
{
	if (const UFrameRateSettingsSubsystem* frame_rate = GetGameInstance() ? GetGameInstance()->GetSubsystem<UFrameRateSettingsSubsystem>() : nullptr)
	{
		lobby_frame_row_->SetOption(frame_rate->GetLobbyOption());
		game_frame_row_->SetOption(frame_rate->GetGameOption());
	}
	const UGameUserSettings* settings = UGameUserSettings::GetGameUserSettings();
	for (int32 option = 0; option < GRAPHICS_QUALITY_OPTION_COUNT; ++option)
	{
		quality_rows_[option]->SetOption((settings->*GRAPHICS_QUALITY_OPTIONS[option].get)() - ENGINE_QUALITY_OFFSET + GetLeadingSteps(option));
	}
	const UFoliageShadowSubsystem* foliage_shadow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UFoliageShadowSubsystem>() : nullptr;
	if (foliage_shadow && foliage_shadow->IsFoliageShadowOff())
	{
		quality_rows_[SHADOW_OPTION]->SetOption(0);
	}
	UpdateOverallQualityRow();
	// 해상도 값 0은 엔진 기본(100%)이다. 가장 가까운 단계로 보여 준다.
	float scale_normalized = 0.0f, scale = 0.0f, min_scale = 0.0f, max_scale = 0.0f;
	settings->GetResolutionScaleInformationEx(scale_normalized, scale, min_scale, max_scale);
	scale = scale <= 0.0f ? RESOLUTION_SCALES[QUALITY_LEVEL_COUNT - 1] : scale;
	int32 resolution_level = 0;
	for (int32 level = 1; level < QUALITY_LEVEL_COUNT; ++level)
	{
		if (FMath::Abs(RESOLUTION_SCALES[level] - scale) < FMath::Abs(RESOLUTION_SCALES[resolution_level] - scale))
		{
			resolution_level = level;
		}
	}
	resolution_row_->SetOption(resolution_level);
}

void USettingsWidget::UpdateOverallQualityRow()
{
	// 그림자 "끔"은 다른 항목의 어느 단계와도 같지 않아 사용자 지정이 된다.
	bool is_same = true;
	for (int32 option = 0; option < quality_rows_.Num(); ++option)
	{
		is_same &= quality_rows_[option]->GetOption() - GetLeadingSteps(option) == quality_rows_[0]->GetOption();
	}
	if (is_same)
	{
		overall_quality_row_->SetOption(quality_rows_[0]->GetOption());
	}
	else
	{
		overall_quality_row_->SetCustomText(FText::FromString(TEXT("사용자 지정")));
	}
}

void USettingsWidget::StepLobbyFrame(int32 step)
{
	lobby_frame_row_->SetOption(FMath::Clamp(lobby_frame_row_->GetOption() + step, 0, UFrameRateSettingsSubsystem::LOBBY_OPTION_COUNT - 1));
}

void USettingsWidget::StepGameFrame(int32 step)
{
	game_frame_row_->SetOption(FMath::Clamp(game_frame_row_->GetOption() + step, 0, UFrameRateSettingsSubsystem::GAME_OPTION_COUNT - 1));
}

void USettingsWidget::StepResolution(int32 step)
{
	resolution_row_->SetOption(FMath::Clamp(resolution_row_->GetOption() + step, 0, QUALITY_LEVEL_COUNT - 1));
}

// 사용자 지정에서 왼쪽은 지금 가장 낮은 단계, 오른쪽은 가장 높은 단계로 모두 맞춘다.
void USettingsWidget::StepOverallQuality(int32 step)
{
	int32 level = overall_quality_row_->GetOption();
	if (level == INDEX_NONE)
	{
		level = step < 0 ? QUALITY_LEVEL_COUNT - 1 : 0;
		for (int32 option = 0; option < quality_rows_.Num(); ++option)
		{
			const int32 row_level = FMath::Max(quality_rows_[option]->GetOption() - GetLeadingSteps(option), 0);
			level = step < 0 ? FMath::Min(level, row_level) : FMath::Max(level, row_level);
		}
	}
	else
	{
		level = FMath::Clamp(level + step, 0, QUALITY_LEVEL_COUNT - 1);
	}
	for (int32 option = 0; option < quality_rows_.Num(); ++option)
	{
		quality_rows_[option]->SetOption(level + GetLeadingSteps(option));
	}
	UpdateOverallQualityRow();
}

void USettingsWidget::StepQuality(int32 step, int32 option)
{
	quality_rows_[option]->SetOption(FMath::Clamp(quality_rows_[option]->GetOption() + step, 0, QUALITY_LEVEL_COUNT - 1 + GetLeadingSteps(option)));
	UpdateOverallQualityRow();
}

void USettingsWidget::ApplySettings()
{
	if (UControlSettingsSubsystem* control = GetGameInstance() ? GetGameInstance()->GetSubsystem<UControlSettingsSubsystem>() : nullptr)
	{
		control->SetMouseSensitivity(mouse_sensitivity_->GetValue());
		control->SetZoomSensitivity(zoom_sensitivity_->GetValue());
		control->SaveConfig();
	}
	if (UAudioSettingsSubsystem* audio = GetGameInstance() ? GetGameInstance()->GetSubsystem<UAudioSettingsSubsystem>() : nullptr)
	{
		audio->SetVolumes(master_volume_->GetValue(), music_volume_->GetValue(), effects_volume_->GetValue());
		audio->SaveConfig();
	}
	capturing_action_ = INDEX_NONE;
	GetGameInstance()->GetSubsystem<UKeyBindingSubsystem>()->SetKeys(pending_keys_);
	UpdateKeyTexts();
	UGameUserSettings* settings = UGameUserSettings::GetGameUserSettings();
	// 그림자 "끔"은 엔진 그림자 품질을 낮음으로 두고 폴리지 그림자를 끈다.
	for (int32 option = 0; option < GRAPHICS_QUALITY_OPTION_COUNT; ++option)
	{
		(settings->*GRAPHICS_QUALITY_OPTIONS[option].set)(FMath::Max(quality_rows_[option]->GetOption() - GetLeadingSteps(option), 0) + ENGINE_QUALITY_OFFSET);
	}
	if (UFoliageShadowSubsystem* foliage_shadow = GetGameInstance() ? GetGameInstance()->GetSubsystem<UFoliageShadowSubsystem>() : nullptr)
	{
		foliage_shadow->SetFoliageShadowOff(quality_rows_[SHADOW_OPTION]->GetOption() == 0);
	}
	settings->SetResolutionScaleValueEx(RESOLUTION_SCALES[resolution_row_->GetOption()]);
	settings->ApplyNonResolutionSettings();
	settings->SaveSettings();
	// 그래픽 설정을 적용하면 엔진이 최대 프레임을 사용자 설정 값으로 되돌리므로 프레임 제한은 그 뒤에 맞춘다.
	if (UFrameRateSettingsSubsystem* frame_rate = GetGameInstance() ? GetGameInstance()->GetSubsystem<UFrameRateSettingsSubsystem>() : nullptr)
	{
		frame_rate->SetLobbyOption(lobby_frame_row_->GetOption());
		frame_rate->SetGameOption(game_frame_row_->GetOption());
	}
	UpdateGraphicsRows();
}

void USettingsWidget::ResetToDefaults()
{
	mouse_sensitivity_->SetValue(UControlSettingsSubsystem::DEFAULT_SENSITIVITY);
	zoom_sensitivity_->SetValue(UControlSettingsSubsystem::DEFAULT_SENSITIVITY);
	for (USlider* slider : { master_volume_.Get(), music_volume_.Get(), effects_volume_.Get() })
	{
		slider->SetValue(UAudioSettingsSubsystem::DEFAULT_VOLUME);
	}
	UpdateVolumeLabels(0.0f);
	for (int32 action = 0; action < pending_keys_.Num(); ++action)
	{
		pending_keys_[action] = UKeyBindingSubsystem::GetDefaultKey(static_cast<EKeyAction>(action));
	}
	capturing_action_ = INDEX_NONE;
	UpdateKeyTexts();
	lobby_frame_row_->SetOption(UFrameRateSettingsSubsystem::DEFAULT_LOBBY_OPTION);
	game_frame_row_->SetOption(UFrameRateSettingsSubsystem::DEFAULT_GAME_OPTION);
	resolution_row_->SetOption(DEFAULT_RESOLUTION_LEVEL);
	for (int32 option = 0; option < quality_rows_.Num(); ++option)
	{
		quality_rows_[option]->SetOption(DEFAULT_QUALITY_LEVEL + GetLeadingSteps(option));
	}
	if (UFoliageShadowSubsystem::DEFAULT_IS_FOLIAGE_SHADOW_OFF)
	{
		quality_rows_[SHADOW_OPTION]->SetOption(0);
	}
	UpdateOverallQualityRow();
}
