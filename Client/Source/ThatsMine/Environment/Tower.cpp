#include "Tower.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "settings.h"

ATower::ATower()
{
	PrimaryActorTick.bCanEverTick = false;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> tower_mesh(TEXT("/Game/Thats_Mine/Actors/Environments/Asian_Village/SM_tower_04/SM_tower_04.SM_tower_04"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> foundation_mesh(TEXT("/Game/Thats_Mine/Actors/Environments/Asian_Village/SM_tower_foundation_03/SM_tower_foundation_03.SM_tower_foundation_03"));
	tower_mesh_ = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TowerMesh"));
	tower_mesh_->SetStaticMesh(tower_mesh.Object);
	SetRootComponent(tower_mesh_);
	// 기초는 탑 바닥 바로 아래로 내려가고, 가로·세로를 넓혀 탑 돌 받침 폭에 맞춘다. 서버 지형 판정과 같은 크기다.
	foundation_mesh_ = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FoundationMesh"));
	foundation_mesh_->SetStaticMesh(foundation_mesh.Object);
	foundation_mesh_->SetupAttachment(tower_mesh_);
	foundation_mesh_->SetRelativeLocation(FVector(0.0, 0.0, -TOWER_FOUNDATION_HEIGHT));
	foundation_mesh_->SetRelativeScale3D(FVector(TOWER_FOUNDATION_WIDTH_SCALE, TOWER_FOUNDATION_WIDTH_SCALE, 1.0));
}
