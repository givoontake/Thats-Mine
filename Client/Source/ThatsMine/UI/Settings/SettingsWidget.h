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
// 열 때마다 저장된 감도·그래픽 값으로 맞추고, 바꾼 값은 창에만 보이다가 적용을 누르면 반영하고 저장한다. 적용하지 않고 닫으면 바꾼 값은 버린다.
// 기본값을 누르면 감도·그래픽 항목을 처음 값으로 되돌려 보이고, 이것도 적용을 눌러야 반영한다.
// 슬라이더 옆 숫자는 눌러서 직접 입력할 수 있다. 소리는 정수(0~100), 감도는 소수 첫째 자리까지이며 입력을 마치면 범위 안으로 맞춘다.
// 그래픽 탭은 위에서부터 로비·게임 프레임 제한, 렌더 해상도(50·75·100%), 전체 품질, 품질 묶음 10개(엔진의 중간·높음·최고)이고 모두 좌우 화살표로 고른다.
// 전체 품질은 품질 묶음 10개를 한 번에 바꾸고, 10개가 서로 다르면 사용자 지정으로 보인다.
// 마우스를 올린 그래픽 항목의 설명을 창 아래 설명 칸에 보인다.
// 조작 탭은 감도 아래에 동작마다 키 버튼을 두고, 버튼을 누른 뒤 누른 키로 바꾼다. 이미 다른 동작이 쓰는 키면 두 동작의 키를 맞바꾼다.
UCLASS(Abstract)
class THATSMINE_API USettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 게임 중에 연 창을 ESC로 닫으라고 알린다. 로비에서는 묶지 않는다.
	FOnSettingsCloseRequested on_close_requested_;

	// 저장된 감도·그래픽 값으로 창을 맞춘다. 적용하지 않은 값은 버려진다. 열 때마다 부른다.
	void LoadSavedSettings();

	// 탭 모양으로 버튼을 칠한다. 고른 버튼은 선택 배경과 눌림 글자색을 쓴다. 그래픽 탭의 단계 버튼도 같이 쓴다.
	static void ApplyTabStyle(UButton* button, const FButtonStyle& tab_style, const FSlateBrush& selected_brush, bool is_selected);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
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

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> defaults_button_;
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> apply_button_;

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
	void UpdateSensitivityLabels(float value);
	UFUNCTION()
	void CommitSensitivityText(const FText& text, ETextCommit::Type commit_type);
	// 창에 보이는 감도·그래픽 값을 반영하고 저장한다.
	UFUNCTION()
	void ApplySettings();
	// 감도·그래픽 항목을 처음 값으로 되돌려 보인다.
	UFUNCTION()
	void ResetToDefaults();
	// 사용자 설정의 현재 그래픽 값으로 각 줄의 단계를 맞춘다.
	void UpdateGraphicsRows();
	// 품질 묶음 10개가 모두 같으면 그 단계, 다르면 사용자 지정으로 전체 품질을 보인다.
	void UpdateOverallQualityRow();
	// 조작 탭 감도 칸을 스크롤 상자 맨 위로 옮기고, 그 아래에 동작마다 키 줄을 만든다.
	void BuildKeyRows();
	// 마우스가 올라간 키 버튼의 동작을 바꿀 키를 기다린다.
	UFUNCTION()
	void StartKeyCapture();
	void UpdateKeyTexts();
	// 줄의 화살표가 알린 방향(-1, +1)으로 값을 옮긴다.
	void StepLobbyFrame(int32 step);
	void StepGameFrame(int32 step);
	void StepResolution(int32 step);
	void StepOverallQuality(int32 step);
	void StepQuality(int32 step, int32 option);
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

	// 동작 순서의 키 버튼과 그 글자, 창에 보이는 키. 적용을 누르면 키 설정에 넣는다.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> key_buttons_;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> key_texts_;
	TArray<FKey> pending_keys_;
	// 바꿀 키를 기다리는 동작 번호. 기다리지 않으면 INDEX_NONE이다.
	int32 capturing_action_ = INDEX_NONE;
};
