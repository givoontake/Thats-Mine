#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "FootstepNotify.generated.h"

// 이동 애니메이션에서 발이 땅에 닿는 순간. 캐릭터가 발밑 바닥에 맞는 발소리를 재생한다.
UCLASS(meta = (DisplayName = "Footstep"))
class THATSMINE_API UFootstepNotify : public UAnimNotify
{
	GENERATED_BODY()

public:
	UFootstepNotify();
	virtual void Notify(USkeletalMeshComponent* mesh_component, UAnimSequenceBase* animation, const FAnimNotifyEventReference& event_reference) override;
	virtual FString GetNotifyName_Implementation() const override;
};
