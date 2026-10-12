#include "TreasureChest.h"

#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"

ATreasureChest::ATreasureChest()
{
	PrimaryActorTick.bCanEverTick = false;
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> chest_mesh(TEXT("/Game/Thats_Mine/Meshes/Props/Chests/SK_Chest_Gold.SK_Chest_Gold"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> open_animation(TEXT("/Game/Thats_Mine/Meshes/Props/Chests/A_Chest_Open60.A_Chest_Open60"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> pillar_mesh(TEXT("/Game/Thats_Mine/Effects/Treasure/SM_TreasurePillar.SM_TreasurePillar"));
	open_animation_ = open_animation.Object;
	open_effect_ = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Thats_Mine/Effects/Chest/NS_ChestGoldBurst.NS_ChestGoldBurst")));

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	chest_ = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Chest"));
	chest_->SetupAttachment(RootComponent);
	chest_->SetSkeletalMesh(chest_mesh.Object);
	chest_->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	chest_->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	// 빛기둥은 바닥에서 위로 선다. 원기둥 메시는 가운데가 원점이라 높이의 절반만큼 올린다.
	light_pillar_ = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LightPillar"));
	light_pillar_->SetupAttachment(RootComponent);
	light_pillar_->SetStaticMesh(pillar_mesh.Object);
	light_pillar_->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	light_pillar_->SetCastShadow(false);
	light_pillar_->SetRelativeLocation(FVector(0.0, 0.0, PILLAR_HEIGHT_SCALE * 50.0));
	light_pillar_->SetRelativeScale3D(FVector(PILLAR_DIAMETER_SCALE, PILLAR_DIAMETER_SCALE, PILLAR_HEIGHT_SCALE));
}

void ATreasureChest::BeginPlay()
{
	Super::BeginPlay();
	chest_->PlayAnimation(open_animation_, false);
	const FBoxSphereBounds bounds = chest_->Bounds;
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, open_effect_.LoadSynchronous(), bounds.Origin + FVector(0.0, 0.0, bounds.BoxExtent.Z * 0.25), FRotator::ZeroRotator, FVector::OneVector, true, true, ENCPoolMethod::AutoRelease);
}
