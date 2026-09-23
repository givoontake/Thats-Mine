#include "MapBoundarySubsystem.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

void UMapBoundarySubsystem::OnWorldBeginPlay(UWorld& world)
{
	Super::OnWorldBeginPlay(world);
	if (world.GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	UMaterialInterface* boundary_material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Thats_Mine/Materials/M_MapBoundaryBlue.M_MapBoundaryBlue"));
	if (!boundary_material)
	{
		return;
	}
	float opacity = 0.0f;
	boundary_material->GetScalarParameterValue(FMaterialParameterInfo(TEXT("Opacity")), opacity);
	for (TActorIterator<AStaticMeshActor> actor(&world); actor; ++actor)
	{
		UStaticMeshComponent* component = actor->GetStaticMeshComponent();
		if (component->GetMaterial(0) != boundary_material || !component->GetStaticMesh())
		{
			continue;
		}
		UMaterialInstanceDynamic* material = component->CreateDynamicMaterialInstance(0, boundary_material);
		material->SetScalarParameterValue(TEXT("Opacity"), 0.0f);
		boundaries_.Add({component, material, component->GetStaticMesh()->GetBoundingBox(), opacity});
	}
}

void UMapBoundarySubsystem::Deinitialize()
{
	boundaries_.Reset();
	Super::Deinitialize();
}

bool UMapBoundarySubsystem::DoesSupportWorldType(const EWorldType::Type world_type) const
{
	return world_type == EWorldType::Game || world_type == EWorldType::PIE;
}

bool UMapBoundarySubsystem::IsTickable() const
{
	return !boundaries_.IsEmpty();
}

TStatId UMapBoundarySubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMapBoundarySubsystem, STATGROUP_Tickables);
}

void UMapBoundarySubsystem::Tick(float delta_seconds)
{
	Super::Tick(delta_seconds);
	const APlayerController* controller = GetWorld()->GetFirstPlayerController();
	const APawn* pawn = controller && controller->IsLocalController() ? controller->GetPawn() : nullptr;
	for (const FBoundaryFadeEntry& boundary : boundaries_)
	{
		const UStaticMeshComponent* component = boundary.component.Get();
		UMaterialInstanceDynamic* material = boundary.material.Get();
		if (!component || !material)
		{
			continue;
		}
		float fade = 0.0f;
		if (pawn)
		{
			const FVector position = pawn->GetActorLocation();
			const FTransform& transform = component->GetComponentTransform();
			const FVector closest = transform.TransformPosition(boundary.local_bounds.GetClosestPointTo(transform.InverseTransformPosition(position)));
			const float distance = FVector::Distance(position, closest);
			fade = FMath::Clamp(1.0f - distance / FADE_DISTANCE, 0.0f, 1.0f);
		}
		material->SetScalarParameterValue(TEXT("Opacity"), boundary.opacity * fade);
	}
}
