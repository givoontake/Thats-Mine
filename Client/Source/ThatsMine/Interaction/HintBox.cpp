#include "HintBox.h"

#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AHintBox::AHintBox()
{
	PrimaryActorTick.bCanEverTick = false;
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> chest_mesh(TEXT("/Game/Thats_Mine/Meshes/Props/Chests/SK_Chest.SK_Chest"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> open_animation(TEXT("/Game/Thats_Mine/Meshes/Props/Chests/A_Chest_Open60.A_Chest_Open60"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> open_mesh(TEXT("/Game/Thats_Mine/Meshes/Props/Chests/SM_Chest_Open60.SM_Chest_Open60"));
	chest_mesh_ = chest_mesh.Object;
	open_animation_ = open_animation.Object;
	open_mesh_ = open_mesh.Object;
	open_effect_ = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(TEXT("/Game/Thats_Mine/Effects/Chest/NS_ChestGoldBurst.NS_ChestGoldBurst")));
}

void AHintBox::Activate(bool is_shot)
{
	const bool is_gift = ActorHasTag(TEXT("HintGift"));
	if (is_activated_ || (is_shot && !is_gift))
	{
		return;
	}
	is_activated_ = true;
	UStaticMeshComponent* mesh = GetStaticMeshComponent();
	const FBoxSphereBounds bounds = mesh->GetStaticMesh()->GetBounds();
	const FVector effect_origin = mesh->GetComponentTransform().TransformPosition(bounds.Origin + FVector(0.0, 0.0, is_gift ? 0.0 : bounds.BoxExtent.Z * 0.25));
	// 상자가 없어져도 효과는 월드에 남아 1초 동안 재생된다.
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, open_effect_.LoadSynchronous(), effect_origin, FRotator::ZeroRotator, GetActorScale3D(), true, true, ENCPoolMethod::AutoRelease);
	if (is_gift)
	{
		SetActorEnableCollision(false);
		SetActorHiddenInGame(true);
		Destroy();
		return;
	}

	// 열리는 0.5초 동안만 스켈레탈 메시를 사용한다.
	animated_mesh_ = NewObject<USkeletalMeshComponent>(this);
	animated_mesh_->SetupAttachment(mesh);
	animated_mesh_->SetSkeletalMesh(chest_mesh_);
	animated_mesh_->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	animated_mesh_->SetGenerateOverlapEvents(false);
	animated_mesh_->SetCanEverAffectNavigation(false);
	animated_mesh_->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	animated_mesh_->RegisterComponent();
	mesh->SetVisibility(false);
	animated_mesh_->PlayAnimation(open_animation_, false);
	animated_mesh_->SetPlayRate(open_animation_->GetPlayLength() / OPEN_DURATION);
	GetWorldTimerManager().SetTimer(open_timer_, this, &AHintBox::FinishOpening, OPEN_DURATION, false);
}

void AHintBox::FinishOpening()
{
	UStaticMeshComponent* mesh = GetStaticMeshComponent();
	mesh->SetMobility(EComponentMobility::Movable);
	mesh->SetStaticMesh(open_mesh_);
	mesh->SetVisibility(true);
	animated_mesh_->DestroyComponent();
	animated_mesh_ = nullptr;
}

void AHintBox::EndPlay(const EEndPlayReason::Type end_play_reason)
{
	GetWorldTimerManager().ClearTimer(open_timer_);
	Super::EndPlay(end_play_reason);
}
