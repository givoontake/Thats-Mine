#include "StartBarrier.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Flow/ClientStateSubsystem.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AStartBarrier::AStartBarrier()
{
	PrimaryActorTick.bCanEverTick = false;
	barrier_mesh_ = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BarrierMesh"));
	SetRootComponent(barrier_mesh_);
	// 게임 시작 전까지 캐릭터 이동만 막는다. 충돌은 벽이 보이는 동안에만 켜고, 에디터에서는 꺼 두어 서버 맵 데이터에 들어가지 않는다.
	barrier_mesh_->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	barrier_mesh_->SetCollisionResponseToAllChannels(ECR_Ignore);
	barrier_mesh_->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	barrier_mesh_->SetGenerateOverlapEvents(false);
	barrier_mesh_->SetCastShadow(false);
	barrier_mesh_->SetCanEverAffectNavigation(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> material(TEXT("/Game/Thats_Mine/Materials/M_StartBarrier.M_StartBarrier"));
	barrier_mesh_->SetStaticMesh(cube.Object);
	barrier_mesh_->SetMaterial(0, material.Object);
}

void AStartBarrier::BeginPlay()
{
	Super::BeginPlay();
	const UGameInstance* game_instance = GetGameInstance();
	if (game_instance && game_instance->GetSubsystem<UClientStateSubsystem>()->IsLocalMode())
	{
		HideBarrier();
	}
	else
	{
		ShowBarrier();
	}
}

void AStartBarrier::ShowBarrier()
{
	barrier_mesh_->SetVisibility(true);
	barrier_mesh_->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void AStartBarrier::HideBarrier()
{
	barrier_mesh_->SetVisibility(false);
	barrier_mesh_->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
