#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "boundary_fade_entry.h"
#include "MapBoundarySubsystem.generated.h"

UCLASS()
class UNREAL_PRACTICE_API UMapBoundarySubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& world) override;
	virtual void Deinitialize() override;
	virtual void Tick(float delta_seconds) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type world_type) const override;

private:
	static constexpr float FADE_DISTANCE = 1000.0f;
	TArray<FBoundaryFadeEntry> boundaries_;
};
