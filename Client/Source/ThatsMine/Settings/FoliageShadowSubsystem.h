#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/World.h"
#include "FoliageShadowSubsystem.generated.h"

// 폴리지 그림자 설정. 그래픽 탭 그림자 항목의 "끔" 단계가 이 설정이고, 끄면 엔진 그림자 품질을 가장 낮은 단계(LOWEST_SHADOW_QUALITY)로 두고
// 칠한 폴리지(나무·바위·꽃 등)와 지형 풀의 그림자를 그리지 않는다. 레벨의 배치 액터가 준비될 때와 설정을 바꿀 때 적용하고,
// 내 컴퓨터의 사용자 설정 파일(GameUserSettings.ini)에 저장한다. 처음 값은 끔이다.
UCLASS(Config = GameUserSettings)
class THATSMINE_API UFoliageShadowSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static constexpr bool DEFAULT_IS_FOLIAGE_SHADOW_OFF = true;
	// 끔일 때 쓰는 엔진 그림자 품질. 설정 화면의 "낮음"과 같다.
	static constexpr int32 LOWEST_SHADOW_QUALITY = 1;

	virtual void Initialize(FSubsystemCollectionBase& collection) override;
	virtual void Deinitialize() override;

	bool IsFoliageShadowOff() const { return is_foliage_shadow_off_; }
	void SetFoliageShadowOff(bool is_foliage_shadow_off);

private:
	void HandleActorsInitialized(const FActorsInitializedParams& params);
	// 월드의 폴리지 컴포넌트마다 폴리지 종류의 그림자 설정을 따르거나, 끔이면 그림자를 끈다.
	void ApplyToWorld(UWorld* world) const;

	UPROPERTY(Config)
	bool is_foliage_shadow_off_ = DEFAULT_IS_FOLIAGE_SHADOW_OFF;

	FDelegateHandle actors_initialized_handle_;
};
