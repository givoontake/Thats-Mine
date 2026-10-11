#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ControlSettingsSubsystem.generated.h"

// 조작 설정(일반 마우스 감도, 줌 마우스 감도). 내 컴퓨터의 사용자 설정 파일(GameUserSettings.ini)에 저장한다.
UCLASS(Config = GameUserSettings)
class THATSMINE_API UControlSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static constexpr float MIN_SENSITIVITY = 0.1f;
	static constexpr float MAX_SENSITIVITY = 10.0f;
	static constexpr float DEFAULT_SENSITIVITY = 1.0f;

	float GetMouseSensitivity() const { return mouse_sensitivity_; }
	float GetZoomSensitivity() const { return zoom_sensitivity_; }
	// 범위 안으로 맞춰 바꾼다. 파일에 쓰는 것은 SaveConfig()를 부를 때다.
	void SetMouseSensitivity(float sensitivity);
	void SetZoomSensitivity(float sensitivity);

private:
	UPROPERTY(Config)
	float mouse_sensitivity_ = DEFAULT_SENSITIVITY;

	UPROPERTY(Config)
	float zoom_sensitivity_ = DEFAULT_SENSITIVITY;
};
