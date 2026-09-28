#include "CombatDebug.h"

#if !UE_BUILD_SHIPPING
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"
#include "Player/shot_result.h"

static TAutoConsoleVariable<int32> DEBUG_MELEE(TEXT("tm.DebugMelee"), 0, TEXT("Draw local melee traces: 0 off, 1 on."), ECVF_Cheat);
static TAutoConsoleVariable<int32> DEBUG_SHOT(TEXT("tm.DebugShot"), 0, TEXT("Draw local shot traces: 0 off, 1 on."), ECVF_Cheat);
static TAutoConsoleVariable<int32> DEBUG_SHOT_IMPACT(TEXT("tm.DebugShotImpact"), 1, TEXT("Draw local shot impact points for 5 seconds: 0 off, 1 on."), ECVF_Cheat);

void CombatDebug::DrawMelee(UWorld* world, const FVector& start, const FVector& end, bool has_hit, const FHitResult& hit)
{
#if ENABLE_DRAW_DEBUG
	if (DEBUG_MELEE.GetValueOnGameThread() == 0)
	{
		return;
	}
	const FColor color = has_hit ? FColor::Green : FColor::Red;
	DrawDebugLine(world, start, has_hit ? hit.ImpactPoint : end, color, false, 2.0f);
	if (has_hit)
	{
		DrawDebugSphere(world, hit.ImpactPoint, 5.0f, 12, FColor::Yellow, false, 2.0f);
		DrawDebugLine(world, hit.ImpactPoint, hit.ImpactPoint + hit.ImpactNormal * 25.0f, FColor::Yellow, false, 2.0f);
		UE_LOG(LogTemp, Log, TEXT("Melee hit: %s at %s"), *GetNameSafe(hit.GetActor()), *hit.ImpactPoint.ToCompactString());
	}
#endif
}

void CombatDebug::DrawShot(UWorld* world, const FVector& body_start, const FShotResult& shot)
{
#if ENABLE_DRAW_DEBUG
	if (DEBUG_SHOT_IMPACT.GetValueOnGameThread() != 0 && shot.has_hit)
	{
		// 표면에 가려지지 않도록 탄착면 바깥으로 1cm만 띄운다.
		DrawDebugPoint(world, shot.hit.ImpactPoint + shot.hit.ImpactNormal, 12.0f, FColor::Red, false, 5.0f);
	}
	if (DEBUG_SHOT.GetValueOnGameThread() == 0)
	{
		return;
	}
	DrawDebugLine(world, body_start, shot.muzzle_location, FColor::Cyan, false, 2.0f);
	DrawDebugLine(world, shot.trace_start, shot.trace_end, shot.has_hit ? FColor::Green : FColor::Red, false, 2.0f);
	if (shot.has_hit)
	{
		DrawDebugSphere(world, shot.hit.ImpactPoint, 5.0f, 12, FColor::Yellow, false, 2.0f);
		DrawDebugLine(world, shot.hit.ImpactPoint, shot.hit.ImpactPoint + shot.hit.ImpactNormal * 25.0f, FColor::Yellow, false, 2.0f);
		UE_LOG(LogTemp, Log, TEXT("Shot hit: %s at %s, muzzle blocked: %d"), *GetNameSafe(shot.hit.GetActor()), *shot.hit.ImpactPoint.ToCompactString(), shot.is_muzzle_blocked);
	}
#endif
}
#endif
