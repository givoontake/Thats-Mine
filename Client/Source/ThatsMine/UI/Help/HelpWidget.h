#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "HelpWidget.generated.h"

class UScrollBox;
class UTexture2D;

DECLARE_DELEGATE(FOnHelpCloseRequested);

// 게임 중 도움말 키로 여는 도움말. 인벤토리와 같은 틀에 조작 키, 게임 목표와 진행, 힌트와 상자, 무기·기절·아이템 설명을 보인다.
// 조작 키는 열 때마다 지금 설정된 키로 보인다. 도움말 키나 ESC로 닫고, 이동과 점프는 게임 입력으로 넘긴다.
UCLASS()
class THATSMINE_API UHelpWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UHelpWidget(const FObjectInitializer& object_initializer);
	FOnHelpCloseRequested on_close_requested_;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& geometry, const FKeyEvent& event) override;
	virtual FReply NativeOnKeyDown(const FGeometry& geometry, const FKeyEvent& event) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& geometry, const FPointerEvent& event) override;

	UPROPERTY(EditDefaultsOnly, Category = "Help")
	TSoftObjectPtr<UTexture2D> frame_texture_;

private:
	// 설명 칸을 지금 키 설정으로 다시 채운다.
	void RefreshContent();
	void AddHeading(const FText& text);
	void AddLine(const FText& text);

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> content_;
};
