#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateTypes.h"
#include "Types/SlateEnums.h"
#include "SettingsWidget.generated.h"

class UButton;
class UCycleOptionRow;
class UEditableText;
class UScrollBox;
class USlider;
class UTextBlock;
class UWidgetSwitcher;

DECLARE_DELEGATE(FOnSettingsCloseRequested);

// 그래픽·소리·조작 설정 창. 로비 설정 메뉴에 들어가 있고, 게임 중에는 ESC로 같은 위젯을 따로 연다.
// 감도 슬라이더는 조작 설정 서브시스템 값으로 열리고, 바꾸면 바로 적용하며 슬라이더를 놓을 때와 창이 닫힐 때 저장한다.
// 슬라이더 옆 숫자는 눌러서 직접 입력할 수 있다. 소리는 정수(0~100), 감도는 소수 첫째 자리까지이며 입력을 마치면 범위 안으로 맞춘다.
// 그래픽 탭은 위에서부터 로비·게임 프레임 제한, 렌더 해상도(50·75·100%), 전체 품질, 품질 묶음 10개(엔진의 중간·높음·최고)이고 모두 좌우 화살표로 고른다.
// 전체 품질은 품질 묶음 10개를 한 번에 바꾸고, 10개가 서로 다르면 사용자 지정으로 보인다. 고르면 바로 적용하고 저장한다.
// 마우스를 올린 그래픽 항목의 설명을 창 아래 설명 칸에 보인다.
UCLASS(Abstract)
class THATSMINE_API USettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 게임 중에 연 창을 ESC로 닫으라고 알린다. 로비에서는 묶지 않는다.
	FOnSettingsCloseRequested on_close_requested_;

	// 탭 모양으로 버튼을 칠한다. 고른 버튼은 선택 배경과 눌림 글자색을 쓴다. 그래픽 탭의 단계 버튼도 같이 쓴다.
	static void ApplyTabStyle(UButton* button, const FButtonStyle& tab_style, const FSlateBrush& selected_brush, bool is_selected);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& geometry, const FKeyEvent& event) override;

	// 탭 버튼의 기본 모양과 선택된 탭 배경. 로비 메뉴 버튼과 같은 모양이다.
	UPROPERTY(EditDefaultsOnly, Category = "Appearance")
	FButtonStyle tab_button_style_;
	UPROPERTY(EditDefaultsOnly, Category = "Appearance")
	FSlateBrush selected_tab_brush_;
	// 그래픽 탭 항목 이름과 값 글꼴. 조작 탭 이름, 탭 버튼 글자와 같다.
	UPROPERTY(EditDefaultsOnly, Category = "Appearance")
	FSlateFontInfo option_label_font_;
	UPROPERTY(EditDefaultsOnly, Category = "Appearance")
	FSlateFontInfo option_level_font_;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> audio_tab_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> controls_tab_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> graphics_tab_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> settings_pages_;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USlider> master_volume_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableText> master_volume_text_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USlider> music_volume_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableText> music_volume_text_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USlider> effects_volume_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableText> effects_volume_text_;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USlider> mouse_sensitivity_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableText> mouse_sensitivity_text_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USlider> zoom_sensitivity_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableText> zoom_sensitivity_text_;

	// 그래픽 탭 항목 줄을 담는다. 줄은 코드에서 만든다.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UScrollBox> graphics_options_;

	// 마우스를 올린 항목의 설명.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> option_description_;

private:
	UFUNCTION()
	void SelectAudioTab();
	UFUNCTION()
	void SelectControlsTab();
	UFUNCTION()
	void SelectGraphicsTab();
	void SelectTab(UButton* button);
	UFUNCTION()
	void UpdateVolumeLabels(float value);
	UFUNCTION()
	void CommitVolumeText(const FText& text, ETextCommit::Type commit_type);
	UFUNCTION()
	void ChangeMouseSensitivity(float value);
	UFUNCTION()
	void ChangeZoomSensitivity(float value);
	void UpdateSensitivityLabels();
	UFUNCTION()
	void CommitSensitivityText(const FText& text, ETextCommit::Type commit_type);
	UFUNCTION()
	void SaveSettings();
	// 사용자 설정의 현재 그래픽 값으로 각 줄의 단계를 맞춘다.
	void UpdateGraphicsRows();
	// 줄의 화살표가 알린 방향(-1, +1)으로 값을 옮긴다.
	void StepLobbyFrame(int32 step);
	void StepGameFrame(int32 step);
	void StepResolution(int32 step);
	void StepOverallQuality(int32 step);
	void StepQuality(int32 step, int32 option);
	void ApplyGraphicsSettings();
	UCycleOptionRow* AddOptionRow(const FText& label, const TArray<FText>& option_names, const FText& description);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UCycleOptionRow>> quality_rows_;

	UPROPERTY(Transient)
	TObjectPtr<UCycleOptionRow> lobby_frame_row_;

	UPROPERTY(Transient)
	TObjectPtr<UCycleOptionRow> game_frame_row_;

	UPROPERTY(Transient)
	TObjectPtr<UCycleOptionRow> resolution_row_;

	UPROPERTY(Transient)
	TObjectPtr<UCycleOptionRow> overall_quality_row_;
};
