#include "FrameRateSettingsSubsystem.h"

#include "DynamicRHI.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Flow/ClientStateSubsystem.h"
#include "GameFramework/GameUserSettings.h"

static constexpr float LOBBY_LIMITS[UFrameRateSettingsSubsystem::LOBBY_OPTION_COUNT] = { 30.0f, 60.0f };
static constexpr float GAME_FIXED_LIMITS[] = { 30.0f, 60.0f, 120.0f };

void UFrameRateSettingsSubsystem::Initialize(FSubsystemCollectionBase& collection)
{
	Super::Initialize(collection);
	lobby_option_ = FMath::Clamp(lobby_option_, 0, LOBBY_OPTION_COUNT - 1);
	game_option_ = FMath::Clamp(game_option_, 0, GAME_OPTION_COUNT - 1);
	FScreenResolutionArray resolutions;
	if (GDynamicRHI && RHIGetAvailableResolutions(resolutions, false))
	{
		for (const FScreenResolutionRHI& resolution : resolutions)
		{
			monitor_max_refresh_rate_ = FMath::Max(monitor_max_refresh_rate_, static_cast<float>(resolution.RefreshRate));
		}
	}
	UClientStateSubsystem* client_state = collection.InitializeDependency<UClientStateSubsystem>();
	client_state->on_state_changed_.AddDynamic(this, &UFrameRateSettingsSubsystem::HandleStateChanged);
	ApplyLimit();
}

void UFrameRateSettingsSubsystem::SetLobbyOption(int32 option)
{
	lobby_option_ = FMath::Clamp(option, 0, LOBBY_OPTION_COUNT - 1);
	SaveConfig();
	ApplyLimit();
}

void UFrameRateSettingsSubsystem::SetGameOption(int32 option)
{
	game_option_ = FMath::Clamp(option, 0, GAME_OPTION_COUNT - 1);
	SaveConfig();
	ApplyLimit();
}

float UFrameRateSettingsSubsystem::GetLobbyLimit(int32 option) const
{
	return LOBBY_LIMITS[FMath::Clamp(option, 0, LOBBY_OPTION_COUNT - 1)];
}

float UFrameRateSettingsSubsystem::GetGameLimit(int32 option) const
{
	if (option == GAME_OPTION_MONITOR)
	{
		return monitor_max_refresh_rate_;
	}
	return option == GAME_OPTION_UNLIMITED ? 0.0f : GAME_FIXED_LIMITS[FMath::Clamp(option, 0, GAME_OPTION_MONITOR - 1)];
}

void UFrameRateSettingsSubsystem::HandleStateChanged(EClientState state)
{
	ApplyLimit();
}

// 그래픽 설정을 적용할 때 엔진이 사용자 설정의 프레임 제한으로 최대 프레임을 다시 맞추므로 그 값도 같이 바꾼다.
// 에디터에서는 엔진 최대 프레임이 에디터 화면에도 남으므로 바꾸지 않는다.
void UFrameRateSettingsSubsystem::ApplyLimit()
{
	if (GIsEditor || !GEngine)
	{
		return;
	}
	const EClientState state = GetGameInstance()->GetSubsystem<UClientStateSubsystem>()->GetState();
	const bool is_game = state == EClientState::LOADING || state == EClientState::PLAYING;
	const float limit = is_game ? GetGameLimit(game_option_) : GetLobbyLimit(lobby_option_);
	UGameUserSettings::GetGameUserSettings()->SetFrameRateLimit(limit);
	GEngine->SetMaxFPS(limit);
}
