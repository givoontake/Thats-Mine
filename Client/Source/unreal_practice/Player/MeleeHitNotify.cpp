#include "MeleeHitNotify.h"

#include "PlayerCharacter.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"

UMeleeHitNotify::UMeleeHitNotify()
{
	// 몽타주 인스턴스 식별값을 포함한 동기 알림으로 처리한다.
	bIsNativeBranchingPoint = true;
#if WITH_EDITORONLY_DATA
	bShouldFireInEditor = false;
#endif
}

void UMeleeHitNotify::BranchingPointNotify(FBranchingPointNotifyPayload& payload)
{
	if (IsValid(payload.SkelMeshComponent))
	{
		if (APlayerCharacter* character = Cast<APlayerCharacter>(payload.SkelMeshComponent->GetOwner()); character && payload.SkelMeshComponent == character->GetMesh())
		{
			character->ProcessMeleeHit(payload.MontageInstanceID);
		}
	}
}

FString UMeleeHitNotify::GetNotifyName_Implementation() const
{
	return TEXT("Melee Hit");
}

#if WITH_EDITOR
bool UMeleeHitNotify::CanBePlaced(UAnimSequenceBase* animation) const
{
	return animation && animation->IsA<UAnimMontage>();
}
#endif
