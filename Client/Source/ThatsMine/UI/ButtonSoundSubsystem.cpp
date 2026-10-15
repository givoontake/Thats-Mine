#include "ButtonSoundSubsystem.h"
#include "ButtonClickInputProcessor.h"

#include "Engine/GameInstance.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

static constexpr TCHAR CLICK_SOUND_PATH[] = TEXT("/Game/Thats_Mine/Effects/Sounds/SW_ButtonClick.SW_ButtonClick");
static const FName BUTTON_WIDGET_TYPE(TEXT("SButton"));

DEFINE_LOG_CATEGORY_STATIC(LogButtonSound, Log, All);

void UButtonSoundSubsystem::Initialize(FSubsystemCollectionBase& collection)
{
	Super::Initialize(collection);
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}
	click_sound_ = LoadObject<USoundBase>(nullptr, CLICK_SOUND_PATH);
	input_processor_ = MakeShared<FButtonClickInputProcessor>([this](const FPointerEvent& event) { HandleMouseButtonDown(event); });
	FSlateApplication::Get().RegisterInputPreProcessor(input_processor_);
}

void UButtonSoundSubsystem::Deinitialize()
{
	if (input_processor_ && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(input_processor_);
	}
	input_processor_.Reset();
	Super::Deinitialize();
}

// 누른 곳의 위젯 경로를 안쪽부터 보며 가장 가까운 버튼이 켜져 있으면 재생한다. 버튼 안의 글자를 눌러도 그 버튼으로 본다.
void UButtonSoundSubsystem::HandleMouseButtonDown(const FPointerEvent& event)
{
	if (!click_sound_ || event.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return;
	}
	FSlateApplication& slate = FSlateApplication::Get();
	const FWidgetPath path = slate.LocateWindowUnderMouse(event.GetScreenSpacePosition(), slate.GetInteractiveTopLevelWindows());
	for (int32 index = path.Widgets.Num() - 1; index >= 0; --index)
	{
		const TSharedRef<SWidget>& widget = path.Widgets[index].Widget;
		if (widget->GetType() == BUTTON_WIDGET_TYPE)
		{
			if (widget->IsEnabled())
			{
				UE_LOG(LogButtonSound, Verbose, TEXT("button click at %s"), *event.GetScreenSpacePosition().ToString());
				UGameplayStatics::PlaySound2D(GetGameInstance(), click_sound_);
			}
			return;
		}
	}
}
