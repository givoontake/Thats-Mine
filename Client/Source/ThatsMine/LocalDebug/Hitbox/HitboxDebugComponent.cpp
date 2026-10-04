#include "HitboxDebugComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"

static const FColor HITBOX_LINE_COLOR = FColor::Green;
static constexpr float HITBOX_LINE_THICKNESS = 1.0f;
static constexpr int32 HITBOX_SPHERE_SEGMENTS = 16;

UHitboxDebugComponent::UHitboxDebugComponent()
{
	// 애니메이션으로 뼈 위치가 정해진 뒤에 그린다.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UHitboxDebugComponent::SetHitbox(USkeletalMeshComponent* mesh, UPhysicsAsset* hitbox_asset)
{
	mesh_ = mesh;
	hitbox_asset_ = hitbox_asset;
}

void UHitboxDebugComponent::TickComponent(float delta_time, ELevelTick tick_type, FActorComponentTickFunction* this_tick_function)
{
	Super::TickComponent(delta_time, tick_type, this_tick_function);
#if ENABLE_DRAW_DEBUG
	if (!mesh_ || !hitbox_asset_)
	{
		return;
	}
	UWorld* world = GetWorld();
	for (const USkeletalBodySetup* body : hitbox_asset_->SkeletalBodySetups)
	{
		const int32 bone = mesh_->GetBoneIndex(body->BoneName);
		if (bone == INDEX_NONE)
		{
			continue;
		}
		// 물리 바디처럼 뼈의 월드 변환(배율 포함)에 도형의 뼈 기준 변환을 곱한다. 몸에 가려지지 않게 맨 앞에 그린다.
		const FTransform bone_to_world = mesh_->GetBoneTransform(bone);
		const float scale = static_cast<float>(bone_to_world.GetMaximumAxisScale());
		for (const FKSphereElem& sphere : body->AggGeom.SphereElems)
		{
			DrawDebugSphere(world, bone_to_world.TransformPosition(sphere.Center), sphere.Radius * scale, HITBOX_SPHERE_SEGMENTS, HITBOX_LINE_COLOR, false, -1.0f, SDPG_Foreground, HITBOX_LINE_THICKNESS);
		}
		for (const FKSphylElem& capsule : body->AggGeom.SphylElems)
		{
			const FTransform shape_to_world = capsule.GetTransform() * bone_to_world;
			DrawDebugCapsule(world, shape_to_world.GetLocation(), (capsule.Length * 0.5f + capsule.Radius) * scale, capsule.Radius * scale, shape_to_world.GetRotation(), HITBOX_LINE_COLOR, false, -1.0f, SDPG_Foreground, HITBOX_LINE_THICKNESS);
		}
		for (const FKBoxElem& box : body->AggGeom.BoxElems)
		{
			const FTransform shape_to_world = box.GetTransform() * bone_to_world;
			DrawDebugBox(world, shape_to_world.GetLocation(), FVector(box.X, box.Y, box.Z) * 0.5f * scale, shape_to_world.GetRotation(), HITBOX_LINE_COLOR, false, -1.0f, SDPG_Foreground, HITBOX_LINE_THICKNESS);
		}
	}
#endif
}
