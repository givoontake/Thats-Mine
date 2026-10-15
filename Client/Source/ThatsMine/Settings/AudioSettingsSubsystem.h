#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AudioSettingsSubsystem.generated.h"

class USoundClass;
class USoundMix;
class UWorld;

// 소리 설정(전체·배경음·효과음 볼륨, 0~1). 내 컴퓨터의 사용자 설정 파일(GameUserSettings.ini)에 저장한다.
// 실행 중에 만든 사운드 믹스를 기본 믹스로 두고, 전체는 SC_Master와 그 아래 전부, 배경음은 SC_Music, 효과음은 SC_Effects와 그 아래 종류별 클래스의 볼륨에 곱한다.
// 레벨을 열 때마다 다시 적용한다.
UCLASS(Config = GameUserSettings)
class THATSMINE_API UAudioSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static constexpr float DEFAULT_VOLUME = 1.0f;

	virtual void Initialize(FSubsystemCollectionBase& collection) override;
	virtual void Deinitialize() override;
	float GetMasterVolume() const { return master_volume_; }
	float GetMusicVolume() const { return music_volume_; }
	float GetEffectsVolume() const { return effects_volume_; }
	// 0~1로 맞춰 바꾸고 바로 적용한다. 파일에 쓰는 것은 SaveConfig()를 부를 때다.
	void SetVolumes(float master_volume, float music_volume, float effects_volume);

private:
	void ApplyVolumes(UWorld* world);

	UPROPERTY(Config)
	float master_volume_ = DEFAULT_VOLUME;

	UPROPERTY(Config)
	float music_volume_ = DEFAULT_VOLUME;

	UPROPERTY(Config)
	float effects_volume_ = DEFAULT_VOLUME;

	UPROPERTY(Transient)
	TObjectPtr<USoundMix> volume_mix_;

	UPROPERTY(Transient)
	TObjectPtr<USoundClass> master_class_;

	UPROPERTY(Transient)
	TObjectPtr<USoundClass> music_class_;

	UPROPERTY(Transient)
	TObjectPtr<USoundClass> effects_class_;

	FDelegateHandle map_loaded_handle_;
};
