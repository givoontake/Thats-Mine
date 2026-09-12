#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "PlayerAnimInstance.generated.h"

class APlayerCharacter;

UCLASS()
class UNREAL_PRACTICE_API UPlayerAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float delta_seconds) override;

protected:
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Player|Animation")
	double speed_ = 0.0;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Player|Animation")
	double direction_ = 0.0;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Player|Animation")
	bool is_in_air_ = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Player|Animation")
	bool is_holding_rifle_ = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Player|Animation")
	bool is_aiming_ = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Player|Animation")
	FVector left_hand_ik_location_ = FVector::ZeroVector;

private:
	TWeakObjectPtr<APlayerCharacter> character_;
};
