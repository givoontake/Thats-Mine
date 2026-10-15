#include "FoliageShadowSubsystem.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "FoliageType.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"
#include "InstancedFoliageActor.h"
#include "LandscapeProxy.h"

void UFoliageShadowSubsystem::Initialize(FSubsystemCollectionBase& collection)
{
	Super::Initialize(collection);
	actors_initialized_handle_ = FWorldDelegates::OnWorldInitializedActors.AddUObject(this, &UFoliageShadowSubsystem::HandleActorsInitialized);
	// 설정 화면을 거치지 않은 처음 값도 "끔" 단계와 같은 엔진 그림자 품질로 맞춘다.
	UGameUserSettings* settings = UGameUserSettings::GetGameUserSettings();
	if (is_foliage_shadow_off_ && settings && settings->GetShadowQuality() != LOWEST_SHADOW_QUALITY)
	{
		settings->SetShadowQuality(LOWEST_SHADOW_QUALITY);
		settings->ApplyNonResolutionSettings();
		settings->SaveSettings();
	}
	SetFoliageShadowOff(is_foliage_shadow_off_);
}

void UFoliageShadowSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldInitializedActors.Remove(actors_initialized_handle_);
	Super::Deinitialize();
}

// 지형 풀은 엔진 설정 하나로 끈다. 칠한 폴리지는 지금 월드에 바로 입히고, 이미 만든 지형 풀은 지우고 다시 만들게 한다.
void UFoliageShadowSubsystem::SetFoliageShadowOff(bool is_foliage_shadow_off)
{
	is_foliage_shadow_off_ = is_foliage_shadow_off;
	SaveConfig();
	IConsoleVariable* grass_shadows = IConsoleManager::Get().FindConsoleVariable(TEXT("grass.DisableDynamicShadows"));
	const bool is_grass_changed = grass_shadows && grass_shadows->GetBool() != is_foliage_shadow_off_;
	if (is_grass_changed)
	{
		grass_shadows->Set(is_foliage_shadow_off_ ? 1 : 0);
	}
	if (UWorld* world = GetGameInstance()->GetWorld())
	{
		ApplyToWorld(world);
		// 풀 배치 정보(풀 맵)는 쿠킹 때 만든 것이라 지우면 다시 만들 수 없으므로 남기고 풀 인스턴스만 지운다.
		for (TActorIterator<ALandscapeProxy> landscape(world); landscape && is_grass_changed; ++landscape)
		{
			landscape->FlushGrassComponents(nullptr, false);
		}
	}
}

void UFoliageShadowSubsystem::HandleActorsInitialized(const FActorsInitializedParams& params)
{
	if (params.World && params.World->GetGameInstance() == GetGameInstance())
	{
		ApplyToWorld(params.World);
	}
}

void UFoliageShadowSubsystem::ApplyToWorld(UWorld* world) const
{
	for (TActorIterator<AInstancedFoliageActor> foliage(world); foliage; ++foliage)
	{
		for (const auto& [foliage_type, info] : foliage->GetFoliageInfos())
		{
			UHierarchicalInstancedStaticMeshComponent* component = info->GetComponent();
			if (foliage_type && component)
			{
				component->SetCastShadow(!is_foliage_shadow_off_ && foliage_type->CastShadow);
			}
		}
	}
}
