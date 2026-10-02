#include "StartDome.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AStartDome::AStartDome()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bAllowTickBeforeBeginPlay = true;
	dome_mesh_ = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DomeMesh"));
	SetRootComponent(dome_mesh_);
	dome_mesh_->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	dome_mesh_->SetGenerateOverlapEvents(false);
	dome_mesh_->SetCastShadow(false);
	dome_mesh_->SetCanEverAffectNavigation(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> material(TEXT("/Game/Thats_Mine/Materials/M_StartDome.M_StartDome"));
	dome_mesh_->SetStaticMesh(sphere.Object);
	dome_material_ = material.Object;
	dome_mesh_->SetMaterial(0, dome_material_);
}

void AStartDome::PostRegisterAllComponents()
{
	Super::PostRegisterAllComponents();
	if (!IsTemplate() && dome_mesh_->IsRegistered())
	{
		UpdateMaterial();
	}
}

void AStartDome::OnConstruction(const FTransform& transform)
{
	Super::OnConstruction(transform);
	dynamic_material_ = nullptr;
	UpdateMaterial();
}

void AStartDome::BeginPlay()
{
	Super::BeginPlay();
	ShowDome();
}

void AStartDome::ShowDome()
{
	is_dissolving_ = false;
	SetActorTickEnabled(false);
	dissolve_progress_ = 0.0f;
	UpdateMaterial();
}

void AStartDome::StartDissolve()
{
	ShowDome();
	is_dissolving_ = true;
	SetActorTickEnabled(true);
}

void AStartDome::HideDome()
{
	is_dissolving_ = false;
	SetActorTickEnabled(false);
	dissolve_progress_ = 1.0f;
	UpdateMaterial();
}

bool AStartDome::ShouldTickIfViewportsOnly() const
{
	return is_dissolving_;
}

void AStartDome::Tick(float delta_seconds)
{
	Super::Tick(delta_seconds);
	dissolve_progress_ = FMath::Min(1.0f, dissolve_progress_ + delta_seconds / FMath::Max(dissolve_duration_, 0.1f));
	UpdateMaterial();
	if (dissolve_progress_ >= 1.0f)
	{
		is_dissolving_ = false;
		SetActorTickEnabled(false);
	}
}

void AStartDome::UpdateMaterial()
{
	if (!dome_material_)
	{
		return;
	}
	if (!dynamic_material_)
	{
		dynamic_material_ = dome_mesh_->CreateDynamicMaterialInstance(0, dome_material_);
	}
	const FBoxSphereBounds bounds = dome_mesh_->CalcBounds(dome_mesh_->GetComponentTransform());
	dynamic_material_->SetScalarParameterValue(TEXT("DomeCenterZ"), bounds.Origin.Z);
	dynamic_material_->SetScalarParameterValue(TEXT("DomeRadius"), FMath::Max(bounds.BoxExtent.Z, 1.0));
	dynamic_material_->SetScalarParameterValue(TEXT("DissolveProgress"), FMath::Clamp(dissolve_progress_, 0.0f, 1.0f));
	dome_mesh_->SetVisibility(dissolve_progress_ < 1.0f);
}
