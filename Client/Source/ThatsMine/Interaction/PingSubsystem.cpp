#include "PingSubsystem.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UObjectIterator.h"
#include "hint_meshes.h"

// 충돌이 없는 데이지·버섯·대나무를 핑 자리에서 찾는 반경(cm)
static constexpr float PING_SEARCH_RADIUS = 100.0f;

// 색 데이지·버섯은 같은 메시에 꽃·갓 재질만 달라, 실행 중에는 첫 재질 이름으로 색을 정한다.
struct PingMaterialKind
{
	const TCHAR* name;
	HintObjectKind kind;
};
static constexpr PingMaterialKind PING_MATERIAL_KINDS[] = {
	{ TEXT("MI_DaisyFlower"), HintObjectKind::DAISY_WHITE },
	{ TEXT("MI_DaisyFlower_Red"), HintObjectKind::DAISY_RED },
	{ TEXT("MI_DaisyFlower_Magenta"), HintObjectKind::DAISY_MAGENTA },
	{ TEXT("MI_DaisyFlower_Blue"), HintObjectKind::DAISY_BLUE },
	{ TEXT("MI_DaisyFlower_Yellow"), HintObjectKind::DAISY_YELLOW },
	{ TEXT("MI_Mushrooms"), HintObjectKind::MUSHROOM_RED },
	{ TEXT("MI_Mushrooms_Purple"), HintObjectKind::MUSHROOM_PURPLE },
	{ TEXT("MI_Mushrooms_Blue"), HintObjectKind::MUSHROOM_BLUE },
};

// 메시 컴포넌트가 그리는 힌트 물체 종류. 재질로 색을 먼저 보고, 아니면 메시 이름으로 정한다.
static TOptional<HintObjectKind> FindHintObjectKind(const UStaticMeshComponent* component)
{
	const UStaticMesh* mesh = component ? component->GetStaticMesh() : nullptr;
	if (!mesh)
	{
		return {};
	}
	if (const UMaterialInterface* material = component->GetMaterial(0))
	{
		const FString material_name = material->GetName();
		for (const PingMaterialKind& entry : PING_MATERIAL_KINDS)
		{
			if (material_name == entry.name)
			{
				return entry.kind;
			}
		}
	}
	const FString mesh_name = mesh->GetName();
	for (const HintMesh& entry : HINT_MESHES)
	{
		if (mesh_name == ANSI_TO_TCHAR(entry.name))
		{
			return entry.kind;
		}
	}
	return {};
}

bool UPingSubsystem::DoesSupportWorldType(const EWorldType::Type world_type) const
{
	return world_type == EWorldType::Game || world_type == EWorldType::PIE;
}

// 맞은 메시가 힌트 물체가 아니면, 핑 자리 PING_SEARCH_RADIUS 안의 충돌 없는 인스턴스 중 가장 가까운 힌트 물체로 정한다.
void UPingSubsystem::PlacePing(const FHitResult& hit)
{
	ping_location_ = hit.ImpactPoint;
	ping_object_kind_ = FindHintObjectKind(Cast<UStaticMeshComponent>(hit.GetComponent()));
	if (ping_object_kind_)
	{
		return;
	}
	double nearest = FMath::Square(PING_SEARCH_RADIUS);
	for (TObjectIterator<UInstancedStaticMeshComponent> component; component; ++component)
	{
		if (component->GetWorld() != GetWorld() || component->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
		{
			continue;
		}
		const TOptional<HintObjectKind> kind = FindHintObjectKind(*component);
		if (!kind)
		{
			continue;
		}
		for (const int32 index : component->GetInstancesOverlappingSphere(hit.ImpactPoint, PING_SEARCH_RADIUS, true))
		{
			FTransform transform;
			component->GetInstanceTransform(index, transform, true);
			const double distance = FVector::DistSquared(transform.GetLocation(), hit.ImpactPoint);
			if (distance <= nearest)
			{
				nearest = distance;
				ping_object_kind_ = kind;
			}
		}
	}
}
