#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NetworkWorldSubsystem.generated.h"

UCLASS()
class THATSMINE_API UNetworkWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& collection) override;
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type world_type) const override;

private:
	void OnWorldTickStart(UWorld* world, ELevelTick tick_type, float delta_seconds);
	FDelegateHandle tick_handle_;
};
