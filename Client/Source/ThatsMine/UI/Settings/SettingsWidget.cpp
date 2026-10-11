#include "SettingsWidget.h"

#include "Components/Button.h"
#include "Components/EditableText.h"
#include "Components/ScrollBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameUserSettings.h"
#include "CycleOptionRow.h"
#include "Settings/FrameRateSettingsSubsystem.h"
#include "Player/ControlSettingsSubsystem.h"

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
	{ TEXT("그림자"), TEXT("그림자의 선명도와 그림자가 보이는 거리입니다."), &UGameUserSettings::GetShadowQuality, &UGameUserSettings::SetShadowQuality },
	{ TEXT("전역 조명"), TEXT("빛이 주변에 튕겨 그늘을 밝히는 간접광 품질입니다."), &UGameUserSettings::GetGlobalIlluminationQuality, &UGameUserSettings::SetGlobalIlluminationQuality },
	{ TEXT("반사"), TEXT("물과 매끈한 표면에 비치는 반사의 품질입니다."), &UGameUserSettings::GetReflectionQuality, &UGameUserSettings::SetReflectionQuality },
	{ TEXT("후처리"), TEXT("빛 번짐, 움직임 흐림, 안개 같은 화면 효과의 품질입니다."), &UGameUserSettings::GetPostProcessingQuality, &UGameUserSettings::SetPostProcessingQuality },
	{ TEXT("텍스처"), TEXT("물체 표면 그림의 해상도입니다."), &UGameUserSettings::GetTextureQuality, &UGameUserSettings::SetTextureQuality },
	{ TEXT("효과"), TEXT("총구 화염, 기절 효과 같은 파티클 효과의 품질입니다."), &UGameUserSettings::GetVisualEffectQuality, &UGameUserSettings::SetVisualEffectQuality },
	{ TEXT("셰이딩"), TEXT("물체 표면의 빛과 재질을 계산하는 정밀도입니다."), &UGameUserSettings::GetShadingQuality, &UGameUserSettings::SetShadingQuality },
	{ TEXT("지형"), TEXT("땅 표면의 세밀함입니다."), &UGameUserSettings::GetLandscapeQuality, &UGameUserSettings::SetLandscapeQuality },
};
static constexpr int32 GRAPHICS_QUALITY_OPTION_COUNT = UE_ARRAY_COUNT(GRAPHICS_QUALITY_OPTIONS);
// 품질 단계 수와 렌더 해상도 단계마다의 화면 비율(%).
static constexpr int32 QUALITY_LEVEL_COUNT = 3;
static constexpr float RESOLUTION_SCALES[QUALITY_LEVEL_COUNT] = { 50.0f, 75.0f, 100.0f };
static constexpr int32 ENGINE_QUALITY_OFFSET = 1;
static const TCHAR* DESCRIPTION_HINT = TEXT("항목에 마우스를 올리면 설명이 표시됩니다.");

void USettingsWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	audio_tab_->OnClicked.AddDynamic(this, &USettingsWidget::SelectAudioTab);
	controls_tab_->OnClicked.AddDynamic(this, &USettingsWidget::SelectControlsTab);
	graphics_tab_->OnClicked.AddDynamic(this, &USettingsWidget::SelectGraphicsTab);
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
		UCycleOptionRow* row = AddOptionRow(FText::FromString(GRAPHICS_QUALITY_OPTIONS[option].label), quality_names, FText::FromString(GRAPHICS_QUALITY_OPTIONS[option].description));
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
		slider->OnMouseCaptureEnd.AddDynamic(this, &USettingsWidget::SaveSettings);
		slider->OnControllerCaptureEnd.AddDynamic(this, &USettingsWidget::SaveSettings);
	}
	mouse_sensitivity_->OnValueChanged.AddDynamic(this, &USettingsWidget::ChangeMouseSensitivity);
	zoom_sensitivity_->OnValueChanged.AddDynamic(this, &USettingsWidget::ChangeZoomSensitivity);
	SelectGraphicsTab();
}

// 열 때마다 저장된 감도로 슬라이더를 맞춘다.
void USettingsWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (const UControlSettingsSubsystem* settings = GetGameInstance() ? GetGameInstance()->GetSubsystem<UControlSettingsSubsystem>() : nullptr)
	{
		mouse_sensitivity_->SetValue(settings->GetMouseSensitivity());
		zoom_sensitivity_->SetValue(settings->GetZoomSensitivity());
	}
	UpdateSensitivityLabels();
	UpdateGraphicsRows();
}

void USettingsWidget::NativeDestruct()
{
	SaveSettings();
	Super::NativeDestruct();
}

FReply USettingsWidget::NativeOnPreviewKeyDown(const FGeometry& geometry, const FKeyEvent& event)
{
	if (event.GetKey() == EKeys::Escape && on_close_requested_.IsBound())
	{
		if (!event.IsRepeat()) { on_close_requested_.Execute(); }
		return FReply::Handled();
	}
	return Super::NativeOnPreviewKeyDown(geometry, event);
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

void USettingsWidget::ChangeMouseSensitivity(float value)
{
	if (UControlSettingsSubsystem* settings = GetGameInstance() ? GetGameInstance()->GetSubsystem<UControlSettingsSubsystem>() : nullptr)
	{
		settings->SetMouseSensitivity(value);
	}
	UpdateSensitivityLabels();
}

void USettingsWidget::ChangeZoomSensitivity(float value)
{
	if (UControlSettingsSubsystem* settings = GetGameInstance() ? GetGameInstance()->GetSubsystem<UControlSettingsSubsystem>() : nullptr)
	{
		settings->SetZoomSensitivity(value);
	}
	UpdateSensitivityLabels();
}

void USettingsWidget::UpdateSensitivityLabels()
{
	FNumberFormattingOptions format;
	format.SetMinimumFractionalDigits(1);
	format.SetMaximumFractionalDigits(1);
	mouse_sensitivity_text_->SetText(FText::AsNumber(mouse_sensitivity_->GetValue(), &format));
	zoom_sensitivity_text_->SetText(FText::AsNumber(zoom_sensitivity_->GetValue(), &format));
}

// 입력한 숫자를 소수 첫째 자리로 반올림하고 범위 안으로 맞춰 슬라이더와 감도에 넣고 저장한다.
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
		if (UControlSettingsSubsystem* settings = GetGameInstance() ? GetGameInstance()->GetSubsystem<UControlSettingsSubsystem>() : nullptr)
		{
			settings->SetMouseSensitivity(mouse_sensitivity_->GetValue());
			settings->SetZoomSensitivity(zoom_sensitivity_->GetValue());
			settings->SaveConfig();
		}
	}
	UpdateSensitivityLabels();
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
	bool is_same = true;
	for (int32 option = 0; option < GRAPHICS_QUALITY_OPTION_COUNT; ++option)
	{
		quality_rows_[option]->SetOption((settings->*GRAPHICS_QUALITY_OPTIONS[option].get)() - ENGINE_QUALITY_OFFSET);
		is_same &= quality_rows_[option]->GetOption() == quality_rows_[0]->GetOption();
	}
	if (is_same)
	{
		overall_quality_row_->SetOption(quality_rows_[0]->GetOption());
	}
	else
	{
		overall_quality_row_->SetCustomText(FText::FromString(TEXT("사용자 지정")));
	}
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

void USettingsWidget::StepLobbyFrame(int32 step)
{
	if (UFrameRateSettingsSubsystem* frame_rate = GetGameInstance() ? GetGameInstance()->GetSubsystem<UFrameRateSettingsSubsystem>() : nullptr)
	{
		frame_rate->SetLobbyOption(frame_rate->GetLobbyOption() + step);
		lobby_frame_row_->SetOption(frame_rate->GetLobbyOption());
	}
}

void USettingsWidget::StepGameFrame(int32 step)
{
	if (UFrameRateSettingsSubsystem* frame_rate = GetGameInstance() ? GetGameInstance()->GetSubsystem<UFrameRateSettingsSubsystem>() : nullptr)
	{
		frame_rate->SetGameOption(frame_rate->GetGameOption() + step);
		game_frame_row_->SetOption(frame_rate->GetGameOption());
	}
}

void USettingsWidget::StepResolution(int32 step)
{
	const int32 level = FMath::Clamp(resolution_row_->GetOption() + step, 0, QUALITY_LEVEL_COUNT - 1);
	UGameUserSettings::GetGameUserSettings()->SetResolutionScaleValueEx(RESOLUTION_SCALES[level]);
	ApplyGraphicsSettings();
}

// 사용자 지정에서 왼쪽은 지금 가장 낮은 단계, 오른쪽은 가장 높은 단계로 모두 맞춘다.
void USettingsWidget::StepOverallQuality(int32 step)
{
	int32 level = overall_quality_row_->GetOption();
	if (level == INDEX_NONE)
	{
		level = step < 0 ? QUALITY_LEVEL_COUNT - 1 : 0;
		for (const UCycleOptionRow* row : quality_rows_)
		{
			level = step < 0 ? FMath::Min(level, row->GetOption()) : FMath::Max(level, row->GetOption());
		}
	}
	else
	{
		level = FMath::Clamp(level + step, 0, QUALITY_LEVEL_COUNT - 1);
	}
	UGameUserSettings* settings = UGameUserSettings::GetGameUserSettings();
	for (const GraphicsQualityOption& option : GRAPHICS_QUALITY_OPTIONS)
	{
		(settings->*option.set)(level + ENGINE_QUALITY_OFFSET);
	}
	ApplyGraphicsSettings();
}

void USettingsWidget::StepQuality(int32 step, int32 option)
{
	const int32 level = FMath::Clamp(quality_rows_[option]->GetOption() + step, 0, QUALITY_LEVEL_COUNT - 1);
	(UGameUserSettings::GetGameUserSettings()->*GRAPHICS_QUALITY_OPTIONS[option].set)(level + ENGINE_QUALITY_OFFSET);
	ApplyGraphicsSettings();
}

void USettingsWidget::ApplyGraphicsSettings()
{
	UGameUserSettings* settings = UGameUserSettings::GetGameUserSettings();
	settings->ApplyNonResolutionSettings();
	settings->SaveSettings();
	UpdateGraphicsRows();
}

void USettingsWidget::SaveSettings()
{
	if (UControlSettingsSubsystem* settings = GetGameInstance() ? GetGameInstance()->GetSubsystem<UControlSettingsSubsystem>() : nullptr)
	{
		settings->SaveConfig();
	}
}
