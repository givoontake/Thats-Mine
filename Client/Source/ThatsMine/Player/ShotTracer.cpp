#include "ShotTracer.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AShotTracer::AShotTracer()
{
	PrimaryActorTick.bCanEverTick = true;
	mesh_ = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = mesh_;
	mesh_->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	mesh_->SetCastShadow(false);
	// 패키지에 함께 들어가도록 생성자에서 원기둥과 머티리얼을 직접 가리킨다.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> material(TEXT("/Game/Thats_Mine/Effects/M_ShotTracer.M_ShotTracer"));
	mesh_->SetStaticMesh(cylinder.Object);
	mesh_->SetMaterial(0, material.Object);
	InitialLifeSpan = LIFE_SECONDS;
}

void AShotTracer::BeginPlay()
{
	Super::BeginPlay();
	material_ = mesh_->CreateDynamicMaterialInstance(0);
}

// 원기둥은 높이 100cm, 지름 100cm에 가운데가 중심이라, 두 점의 가운데에 놓고 높이 축을 두 점 방향으로 돌려 길이만큼 늘린다.
void AShotTracer::SetSegment(const FVector& start, const FVector& end)
{
	const FVector segment = end - start;
	SetActorLocationAndRotation((start + end) * 0.5, FRotationMatrix::MakeFromZ(segment).Rotator());
	SetActorScale3D(FVector(THICKNESS / 100.0, THICKNESS / 100.0, segment.Size() / 100.0));
}

void AShotTracer::FollowMuzzle(USceneComponent* weapon_mesh, FName socket, const FVector& end)
{
	weapon_mesh_ = weapon_mesh;
	socket_ = socket;
	end_ = end;
	SetSegment(weapon_mesh->GetSocketLocation(socket), end);
}

void AShotTracer::Tick(float delta_time)
{
	Super::Tick(delta_time);
	age_ += delta_time;
	if (weapon_mesh_.IsValid())
	{
		SetSegment(weapon_mesh_->GetSocketLocation(socket_), end_);
	}
	if (material_)
	{
		material_->SetScalarParameterValue(TEXT("Fade"), FMath::Clamp(1.0f - age_ / LIFE_SECONDS, 0.0f, 1.0f));
	}
}
