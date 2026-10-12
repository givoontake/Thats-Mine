#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Flow/client_state.h"
#include "FrameRateSettingsSubsystem.generated.h"

// 프레임 제한 설정. 로비 화면(로그인·로비·방)과 게임(로딩·플레이)의 제한을 따로 고르고,
// 클라이언트 상태가 바뀔 때마다 엔진 최대 프레임을 그쪽 값으로 맞춘다. 내 컴퓨터의 사용자 설정 파일(GameUserSettings.ini)에 저장한다.
UCLASS(Config = GameUserSettings)
class THATSMINE_API UFrameRateSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// 로비 선택지: 30, 60. 게임 선택지: 30, 60, 120, 모니터 최대 주사율, 제한 없음.
	static constexpr int32 LOBBY_OPTION_COUNT = 2;
	static constexpr int32 GAME_OPTION_COUNT = 5;
	static constexpr int32 GAME_OPTION_MONITOR = 3;
	static constexpr int32 GAME_OPTION_UNLIMITED = 4;
	// 처음 값. 로비 60, 게임은 모니터에 맞춤이다.
	static constexpr int32 DEFAULT_LOBBY_OPTION = 1;
	static constexpr int32 DEFAULT_GAME_OPTION = GAME_OPTION_MONITOR;

	virtual void Initialize(FSubsystemCollectionBase& collection) override;

	int32 GetLobbyOption() const { return lobby_option_; }
	int32 GetGameOption() const { return game_option_; }
	void SetLobbyOption(int32 option);
	void SetGameOption(int32 option);

	// 선택지의 초당 프레임 수. 0은 제한 없음이다.
	float GetLobbyLimit(int32 option) const;
	float GetGameLimit(int32 option) const;
	// 모니터가 지원하는 가장 높은 주사율. 알 수 없으면 0이다.
	float GetMonitorMaxRefreshRate() const { return monitor_max_refresh_rate_; }

private:
	UFUNCTION()
	void HandleStateChanged(EClientState state);
	void ApplyLimit();

	UPROPERTY(Config)
	int32 lobby_option_ = DEFAULT_LOBBY_OPTION;

	UPROPERTY(Config)
	int32 game_option_ = DEFAULT_GAME_OPTION;

	float monitor_max_refresh_rate_ = 0.0f;
};
