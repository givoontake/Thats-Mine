#include "FootstepNotify.h"

#include "PlayerCharacter.h"
#include "Components/SkeletalMeshComponent.h"

UFootstepNotify::UFootstepNotify()
{
#if WITH_EDITORONLY_DATA
	bShouldFireInEditor = false;
#endif
}

void UFootstepNotify::Notify(USkeletalMeshComponent* mesh_component, UAnimSequenceBase* animation, const FAnimNotifyEventReference& event_reference)
{
	Super::Notify(mesh_component, animation, event_reference);
	APlayerCharacter* character = IsValid(mesh_component) ? Cast<APlayerCharacter>(mesh_component->GetOwner()) : nullptr;
	if (character && mesh_component == character->GetMesh())
	{
		character->PlayFootstep();
	}
}

FString UFootstepNotify::GetNotifyName_Implementation() const
{
	return TEXT("Footstep");
}
