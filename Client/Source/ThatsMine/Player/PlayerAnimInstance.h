#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "locomotion_cardinal.h"
#include "PlayerAnimInstance.generated.h"

class APlayerCharacter;

UCLASS()
class THATSMINE_API UPlayerAnimInstance : public UAnimInstance
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

	// 이동 애니메이션을 고르는 기준 방향과, 실제 이동 방향까지 하체를 돌릴 각도
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Player|Animation")
	ELocomotionCardinal locomotion_cardinal_ = ELocomotionCardinal::FORWARD;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Player|Animation")
	float orientation_warp_angle_ = 0.0f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Player|Animation")
	bool is_in_air_ = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Player|Animation")
	bool is_holding_rifle_ = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Player|Animation")
	bool is_holding_pistol_ = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Player|Animation")
	bool is_aiming_ = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Player|Animation")
	FVector left_hand_ik_location_ = FVector::ZeroVector;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Player|Animation")
	float aim_pitch_ = 0.0f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Player|Animation")
	FRotator spine_aim_rotation_ = FRotator::ZeroRotator;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Player|Animation")
	FRotator chest_aim_rotation_ = FRotator::ZeroRotator;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Head", meta = (ClampMin = "0", ClampMax = "89", Units = "deg"))
	float head_pitch_up_limit_ = 60.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Player|Head", meta = (ClampMin = "0", ClampMax = "89", Units = "deg"))
	float head_pitch_down_limit_ = 45.0f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Player|Animation")
	FVector head_look_location_ = FVector::ZeroVector;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Player|Animation")
	float head_look_alpha_ = 0.0f;

private:
	TWeakObjectPtr<APlayerCharacter> character_;
};
