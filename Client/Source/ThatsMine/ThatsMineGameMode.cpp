// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThatsMineGameMode.h"

#include "Components/CapsuleComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Environment/StartBarrier.h"
#include "Flow/ClientStateSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "spawn_points.h"

AThatsMineGameMode::AThatsMineGameMode()
{
	// stub
}

bool AThatsMineGameMode::PlayerCanRestart_Implementation(APlayerController* player)
{
	// 온라인 게임 레벨에서는 레벨이 열릴 때가 아니라 서버의 스폰 패킷을 받을 때 캐릭터를 만든다.
	const UGameInstance* game_instance = GetGameInstance();
	if (TActorIterator<AStartBarrier>(GetWorld()) && game_instance && !game_instance->GetSubsystem<UClientStateSubsystem>()->IsLocalMode())
	{
		return false;
	}
	return Super::PlayerCanRestart_Implementation(player);
}

APawn* AThatsMineGameMode::SpawnDefaultPawnFor_Implementation(AController* new_player, AActor* start_spot)
{
	if (!TActorIterator<AStartBarrier>(GetWorld()))
	{
		return Super::SpawnDefaultPawnFor_Implementation(new_player, start_spot);
	}
	// 시작 구역 차단벽이 있는 게임 레벨의 로컬 테스트는 첫 자리에서 시작한다.
	const SpawnPoint& point = START_SPAWN_POINTS[0];
	return SpawnDefaultPawnAtTransform(new_player, FindStartTransform(FTransform(FRotator(0.0, point.yaw, 0.0), FVector(point.x, point.y, point.z))));
}

FTransform AThatsMineGameMode::FindStartTransform(const FTransform& spawn) const
{
	FTransform transform = spawn;
	const ACharacter* character = DefaultPawnClass ? Cast<ACharacter>(DefaultPawnClass->GetDefaultObject()) : nullptr;
	if (!character)
	{
		return transform;
	}
	// 스폰 좌표는 바닥보다 높게 잡혀 있다.
	// 캡슐을 위에서 떨어뜨려 실제로 닿는 높이에 생성해 스폰할 때 공중에 뜨거나 땅에 박히지 않게 한다.
	const UCapsuleComponent* capsule = character->GetCapsuleComponent();
	const FCollisionShape shape = FCollisionShape::MakeCapsule(capsule->GetScaledCapsuleRadius(), capsule->GetScaledCapsuleHalfHeight());
	const FVector location = transform.GetLocation();
	FHitResult hit;
	if (GetWorld()->SweepSingleByChannel(hit, location + FVector(0.0, 0.0, 300.0), location - FVector(0.0, 0.0, 1000.0), FQuat::Identity, ECC_Pawn, shape, FCollisionQueryParams(SCENE_QUERY_STAT(StartSpawn), false)) && !hit.bStartPenetrating)
	{
		transform.SetLocation(hit.Location + FVector(0.0, 0.0, 2.0));
	}
	return transform;
}
