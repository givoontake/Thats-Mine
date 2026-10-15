#include "AwardStage.h"

#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "LandscapeProxy.h"
#include "UObject/ConstructorHelpers.h"

AAwardStage::AAwardStage()
{
	PrimaryActorTick.bCanEverTick = false;
	SetActorHiddenInGame(true);
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> block_mesh(TEXT("/Game/Stylized_Egypt/Meshes/building/SM_block_07.SM_block_07"));
	for (int32 rank = 0; rank < RANK_COUNT; ++rank)
	{
		const float height = PODIUM_HEIGHTS[rank];
		// 시상대: 한 변 BLOCK_SIZE인 돌 블록을 순위 높이에 묻는 깊이를 더한 만큼 늘이고 줄인다. 메시 기준점과 관계없이 밑면 가운데를 땅속 깊이에 맞춘다.
		UStaticMeshComponent* podium = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Podium%d"), rank + 1));
		podium->SetupAttachment(GetRootComponent());
		podium->SetStaticMesh(block_mesh.Object);
		const FVector podium_scale(1.0, 1.0, (height + PODIUM_SINK) / BLOCK_SIZE);
		const FBox block_bounds = block_mesh.Object ? block_mesh.Object->GetBoundingBox() : FBox(FVector::ZeroVector, FVector::ZeroVector);
		podium->SetRelativeLocation(FVector(0.0, GetPodiumY(rank), -PODIUM_SINK) - FVector(block_bounds.GetCenter().X, block_bounds.GetCenter().Y, block_bounds.Min.Z) * podium_scale);
		podium->SetRelativeScale3D(podium_scale);
		podium->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		podiums_.Add(podium);
		// 전시용 캐릭터는 시상식에서 모습을 받아 보인다.
		USkeletalMeshComponent* figure = CreateDefaultSubobject<USkeletalMeshComponent>(*FString::Printf(TEXT("Figure%d"), rank + 1));
		figure->SetupAttachment(GetRootComponent());
		figure->SetRelativeLocation(FVector(0.0, GetPodiumY(rank), height));
		figure->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		figure->SetVisibility(false);
		figures_.Add(figure);
	}
	camera_ = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	camera_->SetupAttachment(GetRootComponent());
	camera_->SetRelativeLocation(FVector(-950.0, 0.0, 240.0));
	camera_->SetRelativeRotation(FRotator(6.0, 0.0, 0.0));
	camera_->SetFieldOfView(70.0f);

	for (const TCHAR* path : { TEXT("/Game/Thats_Mine/Animations/Award/AS_FigureSpin.AS_FigureSpin"), TEXT("/Game/Thats_Mine/Animations/Award/AS_BBoy.AS_BBoy"),
		TEXT("/Game/Thats_Mine/Animations/Award/AS_Waving.AS_Waving"), TEXT("/Game/Thats_Mine/Animations/Award/AS_WaveSides.AS_WaveSides") })
	{
		ConstructorHelpers::FObjectFinder<UAnimSequence> pose(path);
		if (pose.Succeeded())
		{
			poses_.Add(pose.Object);
		}
	}
}

void AAwardStage::ShowStage()
{
	SetActorHiddenInGame(false);
	ALandscapeProxy::AddExclusionBox(this, GetComponentsBoundingBox(true).ExpandBy(GRASS_CLEAR_MARGIN));
}

float AAwardStage::GetPodiumY(int32 rank) const
{
	// 카메라가 +X를 보므로 -Y가 화면 왼쪽이다. 2위는 왼쪽, 3위는 오른쪽에 둔다.
	return rank == 0 ? 0.0f : (rank == 1 ? -PODIUM_SPACING : PODIUM_SPACING);
}

void AAwardStage::ShowFigures(int32 count, const USkeletalMeshComponent* template_mesh)
{
	for (int32 rank = 0; rank < RANK_COUNT; ++rank)
	{
		USkeletalMeshComponent* figure = figures_[rank];
		const bool is_shown = rank < count && template_mesh && !poses_.IsEmpty();
		figure->SetVisibility(is_shown);
		if (!is_shown)
		{
			figure->Stop();
			continue;
		}
		figure->SetSkeletalMesh(template_mesh->GetSkeletalMeshAsset());
		for (int32 slot = 0; slot < template_mesh->GetNumMaterials(); ++slot)
		{
			figure->SetMaterial(slot, template_mesh->GetMaterial(slot));
		}
		// 플레이어 캐릭터 메시는 캡슐 기준으로 돌아가 있으므로 같은 회전·배율을 쓰고, 카메라 쪽(-X)을 보게 반 바퀴 더 돌린다.
		figure->SetRelativeRotation(template_mesh->GetRelativeRotation() + FRotator(0.0, 180.0, 0.0));
		figure->SetRelativeScale3D(template_mesh->GetRelativeScale3D());
		figure->PlayAnimation(poses_[FMath::RandRange(0, poses_.Num() - 1)], true);
	}
}

FVector AAwardStage::GetRankLocation(int32 rank) const
{
	return GetActorTransform().TransformPosition(FVector(-BLOCK_SIZE * 0.5, GetPodiumY(rank), PODIUM_HEIGHTS[rank] * 0.5));
}

FVector AAwardStage::GetNameLocation(int32 rank) const
{
	return figures_[rank]->GetComponentLocation() + GetActorUpVector() * NAME_HEIGHT;
}
