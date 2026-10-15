#include "AudioSettingsSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "UObject/UObjectGlobals.h"

static constexpr TCHAR MASTER_CLASS_PATH[] = TEXT("/Game/Thats_Mine/Effects/Sounds/Classes/SC_Master.SC_Master");
static constexpr TCHAR MUSIC_CLASS_PATH[] = TEXT("/Game/Thats_Mine/Effects/Sounds/Classes/SC_Music.SC_Music");
static constexpr TCHAR EFFECTS_CLASS_PATH[] = TEXT("/Game/Thats_Mine/Effects/Sounds/Classes/SC_Effects.SC_Effects");

void UAudioSettingsSubsystem::Initialize(FSubsystemCollectionBase& collection)
{
	Super::Initialize(collection);
	volume_mix_ = NewObject<USoundMix>(this);
	master_class_ = LoadObject<USoundClass>(nullptr, MASTER_CLASS_PATH);
	music_class_ = LoadObject<USoundClass>(nullptr, MUSIC_CLASS_PATH);
	effects_class_ = LoadObject<USoundClass>(nullptr, EFFECTS_CLASS_PATH);
	map_loaded_handle_ = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UAudioSettingsSubsystem::ApplyVolumes);
}

void UAudioSettingsSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(map_loaded_handle_);
	map_loaded_handle_.Reset();
	Super::Deinitialize();
}

void UAudioSettingsSubsystem::SetVolumes(float master_volume, float music_volume, float effects_volume)
{
	master_volume_ = FMath::Clamp(master_volume, 0.0f, 1.0f);
	music_volume_ = FMath::Clamp(music_volume, 0.0f, 1.0f);
	effects_volume_ = FMath::Clamp(effects_volume, 0.0f, 1.0f);
	ApplyVolumes(GetGameInstance()->GetWorld());
}

// 전체 볼륨은 아래 클래스에도 곱해져, 효과음은 전체 × 효과음 × 종류별 가중치로 들린다.
void UAudioSettingsSubsystem::ApplyVolumes(UWorld* world)
{
	if (!world || !volume_mix_ || !master_class_ || !music_class_ || !effects_class_)
	{
		return;
	}
	UGameplayStatics::SetBaseSoundMix(world, volume_mix_);
	UGameplayStatics::SetSoundMixClassOverride(world, volume_mix_, master_class_, master_volume_, 1.0f, 0.0f, true);
	UGameplayStatics::SetSoundMixClassOverride(world, volume_mix_, music_class_, music_volume_, 1.0f, 0.0f, true);
	UGameplayStatics::SetSoundMixClassOverride(world, volume_mix_, effects_class_, effects_volume_, 1.0f, 0.0f, true);
}
