#include "HintObjectSubsystem.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FoliageType.h"
#include "InstancedFoliage.h"
#include "InstancedFoliageActor.h"
#include "hint_meshes.h"

DEFINE_LOG_CATEGORY_STATIC(LogHintObject, Log, All);

bool UHintObjectSubsystem::DoesSupportWorldType(const EWorldType::Type world_type) const
{
	return world_type == EWorldType::Game || world_type == EWorldType::PIE;
}

// 서버가 맵 데이터 항목 이름으로 찾는 것처럼, 폴리지는 폴리지 종류 이름을 먼저 보고 없으면 메시 이름을 본다. 배치물은 메시 이름을 본다.
// 맵 범위 밖의 물체는 서버처럼 넣지 않는다.
void UHintObjectSubsystem::OnWorldBeginPlay(UWorld& world)
{
	Super::OnWorldBeginPlay(world);
	const double start_time = FPlatformTime::Seconds();
	TMap<FString, uint8> kinds;
	for (const HintMesh& entry : HINT_MESHES)
	{
		kinds.Add(ANSI_TO_TCHAR(entry.name), static_cast<uint8>(entry.kind));
	}
	TArray<FHintObjectPoint> points;
	const auto add_location = [&points](const FVector& location, uint8 kind)
	{
		if (location.X >= MAP_MIN[0] && location.X <= MAP_MAX[0] && location.Y >= MAP_MIN[1] && location.Y <= MAP_MAX[1])
		{
			points.Add({ FVector3f(location), kind });
		}
	};
	const auto add_component = [&add_location](const UStaticMeshComponent* component, uint8 kind)
	{
		if (const UInstancedStaticMeshComponent* instanced = Cast<UInstancedStaticMeshComponent>(component))
		{
			FTransform transform;
			for (int32 index = 0; index < instanced->GetInstanceCount(); ++index)
			{
				if (instanced->GetInstanceTransform(index, transform, true))
				{
					add_location(transform.GetLocation(), kind);
				}
			}
			return;
		}
		add_location(component->GetComponentLocation(), kind);
	};
	for (TActorIterator<AActor> actor(&world); actor; ++actor)
	{
		if (AInstancedFoliageActor* foliage_actor = Cast<AInstancedFoliageActor>(*actor))
		{
			foliage_actor->ForEachFoliageInfo([&kinds, &add_component](UFoliageType* foliage_type, FFoliageInfo& info)
			{
				const UHierarchicalInstancedStaticMeshComponent* component = info.GetComponent();
				if (component && component->GetStaticMesh())
				{
					const uint8* kind = kinds.Find(foliage_type->GetName());
					kind = kind ? kind : kinds.Find(component->GetStaticMesh()->GetName());
					if (kind)
					{
						add_component(component, *kind);
					}
				}
				return true;
			});
			continue;
		}
		TInlineComponentArray<UStaticMeshComponent*> components(*actor);
		for (const UStaticMeshComponent* component : components)
		{
			const uint8* kind = component->GetStaticMesh() ? kinds.Find(component->GetStaticMesh()->GetName()) : nullptr;
			if (kind)
			{
				add_component(component, *kind);
			}
		}
	}

	// 칸마다 물체 수를 센 뒤 시작 위치를 정하고 칸 순서로 옮긴다.
	const auto get_cell = [](const FVector3f& location)
	{
		const int32 cell_x = FMath::Clamp(FMath::FloorToInt32((location.X - MAP_MIN[0]) / CELL_SIZE), 0, GRID_X - 1);
		const int32 cell_y = FMath::Clamp(FMath::FloorToInt32((location.Y - MAP_MIN[1]) / CELL_SIZE), 0, GRID_Y - 1);
		return cell_y * GRID_X + cell_x;
	};
	cell_starts_.Init(0, GRID_X * GRID_Y + 1);
	for (const FHintObjectPoint& point : points)
	{
		++cell_starts_[get_cell(point.location) + 1];
	}
	for (int32 cell = 0; cell < GRID_X * GRID_Y; ++cell)
	{
		cell_starts_[cell + 1] += cell_starts_[cell];
	}
	TArray<int32> cursors(cell_starts_.GetData(), GRID_X * GRID_Y);
	points_.SetNumUninitialized(points.Num());
	for (const FHintObjectPoint& point : points)
	{
		points_[cursors[get_cell(point.location)]++] = point;
	}
	UE_LOG(LogHintObject, Display, TEXT("Hint objects: %d (%.1f ms)"), points_.Num(), (FPlatformTime::Seconds() - start_time) * 1000.0);
}

TArray<int32> UHintObjectSubsystem::CountKinds(const FVector& location, float radius) const
{
	TArray<int32> counts;
	counts.Init(0, static_cast<int32>(HintObjectKind::COUNT));
	if (cell_starts_.IsEmpty())
	{
		return counts;
	}
	const int32 center_x = FMath::FloorToInt32((location.X - MAP_MIN[0]) / CELL_SIZE);
	const int32 center_y = FMath::FloorToInt32((location.Y - MAP_MIN[1]) / CELL_SIZE);
	const double radius_sq = FMath::Square(static_cast<double>(radius));
	for (int32 cell_y = FMath::Max(center_y - 1, 0); cell_y <= FMath::Min(center_y + 1, GRID_Y - 1); ++cell_y)
	{
		for (int32 cell_x = FMath::Max(center_x - 1, 0); cell_x <= FMath::Min(center_x + 1, GRID_X - 1); ++cell_x)
		{
			const int32 cell = cell_y * GRID_X + cell_x;
			for (int32 index = cell_starts_[cell]; index < cell_starts_[cell + 1]; ++index)
			{
				const FHintObjectPoint& point = points_[index];
				if (FVector::DistSquared(location, FVector(point.location)) <= radius_sq)
				{
					++counts[point.kind];
				}
			}
		}
	}
	// 색 데이지·버섯은 색을 가리지 않은 종류에도 더한다.
	for (int32 kind = static_cast<int32>(HintObjectKind::DAISY_RED); kind <= static_cast<int32>(HintObjectKind::DAISY_WHITE); ++kind)
	{
		counts[static_cast<int32>(HintObjectKind::DAISY)] += counts[kind];
	}
	for (int32 kind = static_cast<int32>(HintObjectKind::MUSHROOM_RED); kind <= static_cast<int32>(HintObjectKind::MUSHROOM_BLUE); ++kind)
	{
		counts[static_cast<int32>(HintObjectKind::MUSHROOM)] += counts[kind];
	}
	return counts;
}
