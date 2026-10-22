#include "MapAssetCollector.h"
#include "grass_placement.h"
#include "AssetCompilingManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FoliageType.h"
#include "InstancedFoliageActor.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "PhysicsEngine/BodySetup.h"

// 맵에 흩어진 탑(받침 포함). 서버가 게임마다 탑 후보 자리(tower_candidates.txt) 중에서 골라 놓으므로 고정 지형에서 뺀다.
static const TSet<FString> SCATTERED_TOWER_LABELS = { TEXT("SM_tower_10"), TEXT("SM_tower_11"), TEXT("SM_tower_24"), TEXT("SM_tower_25"), TEXT("SM_tower_26"),
	TEXT("SM_tower_27"), TEXT("SM_tower_28"), TEXT("SM_tower_29"), TEXT("SM_tower_30"), TEXT("SM_tower_31"), TEXT("SM_tower_32"), TEXT("SM_tower_33"), TEXT("SM_tower_34"),
	TEXT("SM_tower_foundation_19") };

// 메시 기본 충돌을 따르는 부품은 명령행에서 맵을 불러온 순서에 따라 기본값이 아직 적용되지 않았을 수 있다.
// 게임에서처럼 메시 기본 충돌을 다시 적용한다. 메시를 컴파일하는 중이면 엔진이 적용을 건너뛰므로 컴파일을 먼저 끝낸다.
static void ApplyDefaultCollision(UWorld* world)
{
	FAssetCompilingManager::Get().FinishAllCompilation();
	for (TActorIterator<AActor> iterator(world); iterator; ++iterator)
	{
		TInlineComponentArray<UStaticMeshComponent*> components(*iterator);
		for (UStaticMeshComponent* component : components)
		{
			if (component->bUseDefaultCollision)
			{
				component->UpdateCollisionFromStaticMesh();
			}
		}
	}
}

// 서버 맵 파일에 쓸 충돌 모양이 있으면 true. 삼각형 판정은 메시 충돌 삼각형을, 단순 판정은 구·상자·캡슐·볼록 도형을 쓴다.
static bool HasShapes(const UBodySetup* body, const UStaticMesh* static_mesh)
{
	if (body->GetCollisionTraceFlag() == CTF_UseComplexAsSimple && static_mesh)
	{
		return true;
	}
	const FKAggregateGeom& geometry = body->AggGeom;
	return geometry.SphereElems.Num() + geometry.BoxElems.Num() + geometry.SphylElems.Num() + geometry.ConvexElems.Num() > 0;
}

FMapAssetCollector::FMapAssetCollector(UWorld* world, const FBox2D& play_area)
	: play_area_(play_area)
{
	check(world);
	ApplyDefaultCollision(world);

	// 칠한 폴리지의 충돌 없는 물체는 폴리지 종류 이름(같은 메시를 재질만 바꾼 색 변종을 구분)으로 묶는다.
	TMap<const UActorComponent*, FString> foliage_type_names;
	for (TActorIterator<AInstancedFoliageActor> iterator(world); iterator; ++iterator)
	{
		iterator->ForEachFoliageInfo([&foliage_type_names](UFoliageType* type, FFoliageInfo& info)
		{
			if (info.GetComponent())
			{
				foliage_type_names.Add(info.GetComponent(), type->GetName());
			}
			return true;
		});
	}

	for (TActorIterator<AActor> iterator(world); iterator; ++iterator)
	{
		AActor* actor = *iterator;
		// 선물·보물 상자는 서버가 무작위로 스폰하고, 투명 발판은 게임 시작 때 서버가 생성하므로 고정 지형에서 뺀다.
		const FString actor_class = actor->GetClass()->GetName();
		if (actor_class == TEXT("HintBox") || actor_class == TEXT("BP_InvisiblePlatform_C"))
		{
			++skipped_dynamic_;
			continue;
		}
		if (SCATTERED_TOWER_LABELS.Contains(actor->GetActorLabel()))
		{
			++skipped_towers_;
			continue;
		}
		TInlineComponentArray<UPrimitiveComponent*> components(actor);
		for (UPrimitiveComponent* component : components)
		{
			if (const ULandscapeHeightfieldCollisionComponent* landscape = Cast<ULandscapeHeightfieldCollisionComponent>(component))
			{
				landscapes_.Add(landscape);
				continue;
			}
			uint8 flags = 0;
			if (component->IsCollisionEnabled() && component->GetCollisionEnabled() != ECollisionEnabled::PhysicsOnly)
			{
				flags |= component->GetCollisionResponseToChannel(ECC_Visibility) == ECR_Block ? MAP_BLOCKS_VISIBILITY : 0;
				flags |= component->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block ? MAP_BLOCKS_PAWN : 0;
				flags |= component->Mobility == EComponentMobility::Movable ? MAP_MOVABLE : 0;
			}
			const bool is_blocking = (flags & (MAP_BLOCKS_VISIBILITY | MAP_BLOCKS_PAWN)) != 0;
			UStaticMeshComponent* static_mesh_component = Cast<UStaticMeshComponent>(component);
			UStaticMesh* static_mesh = static_mesh_component ? static_mesh_component->GetStaticMesh() : nullptr;
			if (!is_blocking && !static_mesh)
			{
				continue;
			}
			const FString* type_name = foliage_type_names.Find(component);
			const uint8 kind = type_name ? MAP_ASSET_FOLIAGE : static_mesh ? MAP_ASSET_MESH : MAP_ASSET_COMPONENT;
			const UInstancedStaticMeshComponent* instanced = Cast<UInstancedStaticMeshComponent>(component);

			// 막지 않는 메시는 충돌 없는 물체로 위치만 남긴다.
			if (!is_blocking)
			{
				FMapEntry& entry = FindOrAddEntry(type_name ? *type_name : static_mesh->GetName(), kind);
				if (instanced)
				{
					for (int32 instance = 0; instance < instanced->GetInstanceCount(); ++instance)
					{
						FTransform transform;
						instanced->GetInstanceTransform(instance, transform, true);
						AddDecorLocation(entry, transform.GetLocation());
					}
					continue;
				}
				AddDecorLocation(entry, component->GetComponentLocation());
				continue;
			}

			// 막는 배치는 충돌 모양(메시 또는 부품) 단위로 모은다. 액터로 놓은 것과 폴리지로 칠한 것이 같은 메시면 합쳐진다.
			const UBodySetup* body = component->GetBodySetup();
			if (!body || !HasShapes(body, static_mesh))
			{
				++skipped_no_shape_;
				continue;
			}
			const UObject* key = static_mesh ? static_cast<const UObject*>(static_mesh) : static_cast<const UObject*>(component);
			const int32* found_collision = collision_indices_.Find(key);
			FMapCollision& collision = collisions_[found_collision ? *found_collision : collision_indices_.Add(key, collisions_.Add({ key,
				static_mesh ? static_mesh->GetName() : actor->GetActorNameOrLabel(), static_mesh ? static_mesh->GetPathName() : component->GetPathName(), static_mesh, body }))];
			collision.kinds |= kind;
			if (instanced)
			{
				for (int32 instance = 0; instance < instanced->GetInstanceCount(); ++instance)
				{
					FMapObject object{ flags };
					instanced->GetInstanceTransform(instance, object.transform, true);
					collision.objects.Add(object);
				}
				continue;
			}
			collision.objects.Add({ flags, component->GetComponentTransform() });
		}
	}

	// 충돌 모양 이름을 정한 뒤 그 이름의 항목에 붙인다. 충돌 없는 물체 항목과 이름이 같으면 한 항목이 된다.
	NameCollisions();
	for (int32 index = 0; index < collisions_.Num(); ++index)
	{
		FMapEntry& entry = FindOrAddEntry(collisions_[index].name, collisions_[index].kinds);
		entry.collision = index;
	}
	for (const FString& grass_name : CollectGrassMeshNames(world))
	{
		FindOrAddEntry(grass_name, MAP_ASSET_LANDSCAPE_GRASS);
	}
	entries_.Sort([](const FMapEntry& a, const FMapEntry& b) { return a.name < b.name; });
	entry_indices_.Reset();
	for (int32 index = 0; index < entries_.Num(); ++index)
	{
		entry_indices_.Add(entries_[index].name, index);
	}
}

FMapEntry& FMapAssetCollector::FindOrAddEntry(const FString& name, uint8 kind)
{
	const int32* found = entry_indices_.Find(name);
	FMapEntry& entry = found ? entries_[*found] : entries_[entry_indices_.Add(name, entries_.Add({ name }))];
	entry.kinds |= kind;
	return entry;
}

// 이름이 겹치는 충돌 모양은 경로 순서대로 이름 뒤에 _1, _2를 붙여 폴더가 하나만 가리키게 한다.
void FMapAssetCollector::NameCollisions()
{
	TMap<FString, TArray<int32>> name_groups;
	for (int32 index = 0; index < collisions_.Num(); ++index)
	{
		name_groups.FindOrAdd(collisions_[index].name).Add(index);
	}
	for (TPair<FString, TArray<int32>>& group : name_groups)
	{
		if (group.Value.Num() < 2)
		{
			continue;
		}
		group.Value.Sort([this](int32 a, int32 b) { return collisions_[a].path < collisions_[b].path; });
		for (int32 number = 0; number < group.Value.Num(); ++number)
		{
			collisions_[group.Value[number]].name = FString::Printf(TEXT("%s_%d"), *group.Key, number + 1);
		}
	}
}

void FMapAssetCollector::AddDecorLocation(FMapEntry& entry, const FVector& location)
{
	if (location.X >= play_area_.Min.X && location.X <= play_area_.Max.X && location.Y >= play_area_.Min.Y && location.Y <= play_area_.Max.Y)
	{
		entry.decor_locations.Add(location);
	}
}

void FMapAssetCollector::AddGrassLocations(UWorld* world, const TSet<FString>& entry_names)
{
	TSet<FString> grass_names;
	for (const FMapEntry& entry : entries_)
	{
		if ((entry.kinds & MAP_ASSET_LANDSCAPE_GRASS) && entry_names.Contains(entry.name))
		{
			grass_names.Add(entry.name);
		}
	}
	if (grass_names.IsEmpty())
	{
		return;
	}
	const FGrassLocations grass = ComputeGrassLocations(world, grass_names);
	invalid_grass_component_count_ = grass.invalid_component_count;
	for (const TPair<FString, TArray<FVector>>& pair : grass.locations)
	{
		FMapEntry& entry = entries_[entry_indices_[pair.Key]];
		for (const FVector& location : pair.Value)
		{
			AddDecorLocation(entry, location);
		}
	}
}
