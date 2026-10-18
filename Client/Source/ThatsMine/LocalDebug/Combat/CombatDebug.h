#pragma once

#include "CoreMinimal.h"

class UWorld;
struct FHitResult;
struct FShotResult;

class CombatDebug
{
public:
	static void DrawMelee(UWorld* world, const FVector& start, const FVector& end, bool has_hit, const FHitResult& hit);
	static void DrawShot(UWorld* world, const FShotResult& shot);
};
