#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InteractPromptWidget.generated.h"

class UTextBlock;

// 상자를 열거나 두루마리를 주울 수 있을 때 화면 가운데 아래에 상호작용 키와 안내를 보인다.
UCLASS()
class THATSMINE_API UInteractPromptWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

private:
	// 화면 가운데에서 안내를 내려 두는 거리
	static constexpr float CENTER_OFFSET = 150.0f;
	static constexpr int32 FONT_SIZE = 22;

	// 상호작용 키 글자. 보일 때마다 지금 키 설정으로 바꾼다.
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> key_text_;
};
