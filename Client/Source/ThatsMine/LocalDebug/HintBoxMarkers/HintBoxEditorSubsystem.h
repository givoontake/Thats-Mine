#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "HintBoxEditorSubsystem.generated.h"

class AActor;
class APlayerController;
class UCanvas;

UCLASS()
class THATSMINE_API UHintBoxEditorSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& collection) override;
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type world_type) const override;

private:
#if WITH_EDITOR
	void DrawMarkers(UCanvas* canvas, APlayerController* controller);

	TArray<TWeakObjectPtr<AActor>> boxes_;
	FDelegateHandle draw_handle_;
	FDelegateHandle actor_list_handle_;
	FDelegateHandle actor_added_handle_;
	FDelegateHandle actor_deleted_handle_;
	bool is_cache_dirty_ = true;
#endif
};
