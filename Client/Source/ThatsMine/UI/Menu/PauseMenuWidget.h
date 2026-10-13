#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateTypes.h"
#include "PauseMenuWidget.generated.h"

class UButton;
class UVerticalBox;

DECLARE_DELEGATE(FOnPauseMenuRequested);

// 게임 중 ESC로 여는 메뉴. 화면 가운데에 계속하기·설정·로비로 나가기·게임 종료 버튼을 두고, 누른 버튼을 알린다. ESC를 다시 누르면 계속하기와 같다.
UCLASS()
class THATSMINE_API UPauseMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPauseMenuWidget(const FObjectInitializer& object_initializer);

	FOnPauseMenuRequested on_resume_requested_;
	FOnPauseMenuRequested on_settings_requested_;
	FOnPauseMenuRequested on_leave_requested_;
	FOnPauseMenuRequested on_quit_requested_;

	// 로비로 나가기 버튼을 보일지 정한다. 로컬 플레이에는 로비가 없어 숨긴다.
	void SetCanLeaveToLobby(bool can_leave);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& geometry, const FKeyEvent& event) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& geometry, const FPointerEvent& event) override;

private:
	static constexpr float BUTTON_WIDTH = 300.0f;
	static constexpr float BUTTON_HEIGHT = 60.0f;

	UFUNCTION()
	void RequestResume();
	UFUNCTION()
	void RequestSettings();
	UFUNCTION()
	void RequestLeave();
	UFUNCTION()
	void RequestQuit();
	UButton* AddButton(UVerticalBox* list, const FText& text);

	// 버튼 모양. 로비 메뉴 버튼과 같은 나무 모양이고, 화면을 만들 때 재질을 불러 채운다.
	UPROPERTY(Transient)
	FButtonStyle button_style_;

	UPROPERTY(Transient)
	TObjectPtr<UButton> leave_button_;

	bool can_leave_to_lobby_ = true;
};
