#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ButtonSoundSubsystem.generated.h"

class FButtonClickInputProcessor;
class USoundBase;

// 마우스 왼쪽 버튼으로 게임의 버튼을 누르는 순간 클릭음을 재생한다.
// 버튼마다 소리를 넣지 않고, Slate 입력 전처리기로 위젯보다 먼저 마우스 누름을 받아 누른 곳의 위젯 경로에 켜져 있는 버튼(SButton)이 있는지 한 곳에서 본다.
UCLASS()
class THATSMINE_API UButtonSoundSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& collection) override;
	virtual void Deinitialize() override;

private:
	void HandleMouseButtonDown(const FPointerEvent& event);

	UPROPERTY()
	TObjectPtr<USoundBase> click_sound_;

	TSharedPtr<FButtonClickInputProcessor> input_processor_;
};
