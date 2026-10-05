#include "StartDome.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Flow/ClientStateSubsystem.h"
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
	// 게임 시작 전까지 캐릭터 이동만 돔 안에 가둔다. 충돌은 돔이 보이는 동안에만 켠다.
	// 돔 메시는 표면 삼각형을 양면 충돌로 쓰도록 만든 구라서 안에서 밖으로 나가는 캐릭터를 막는다.
	dome_mesh_->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	dome_mesh_->SetCollisionResponseToAllChannels(ECR_Ignore);
	dome_mesh_->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	dome_mesh_->SetGenerateOverlapEvents(false);
	dome_mesh_->SetCastShadow(false);
	dome_mesh_->SetCanEverAffectNavigation(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> sphere(TEXT("/Game/Thats_Mine/Meshes/SM_StartDome.SM_StartDome"));
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
	// 로컬 플레이는 서버의 게임 시작이 없으므로 처음부터 게임이 시작된 상태로 둔다.
	const UGameInstance* game_instance = GetGameInstance();
	if (game_instance && game_instance->GetSubsystem<UClientStateSubsystem>()->IsLocalMode())
	{
		HideDome();
	}
	else
	{
		ShowDome();
	}
}

void AStartDome::ShowDome()
{
	is_dissolving_ = false;
	SetActorTickEnabled(false);
	dome_mesh_->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	dissolve_progress_ = 0.0f;
	UpdateMaterial();
}

void AStartDome::StartDissolve()
{
	ShowDome();
	is_dissolving_ = true;
	SetActorTickEnabled(true);
	dome_mesh_->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AStartDome::HideDome()
{
	is_dissolving_ = false;
	SetActorTickEnabled(false);
	dome_mesh_->SetCollisionEnabled(ECollisionEnabled::NoCollision);
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
