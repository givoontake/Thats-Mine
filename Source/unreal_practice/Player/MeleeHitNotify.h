#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "MeleeHitNotify.generated.h"

UCLASS(meta = (DisplayName = "Melee Hit"))
class UNREAL_PRACTICE_API UMeleeHitNotify : public UAnimNotify
{
	GENERATED_BODY()

public:
	UMeleeHitNotify();
	virtual void BranchingPointNotify(FBranchingPointNotifyPayload& payload) override;
	virtual FString GetNotifyName_Implementation() const override;

#if WITH_EDITOR
	virtual bool CanBePlaced(UAnimSequenceBase* animation) const override;
#endif
};
