// Copyright Epic Games, Inc. All Rights Reserved.

#include "ThatsMineGameMode.h"

#include "Components/CapsuleComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Environment/StartDome.h"
#include "Flow/ClientStateSubsystem.h"
#include "GameFramework/Character.h"
#include "Network/OnlineSessionSubsystem.h"
#include "spawn_points.h"

AThatsMineGameMode::AThatsMineGameMode()
{
	// stub
}

APawn* AThatsMineGameMode::SpawnDefaultPawnFor_Implementation(AController* new_player, AActor* start_spot)
{
	const TOptional<FTransform> spawn = GetStartDomeSpawn();
	const ACharacter* character = Cast<ACharacter>(GetDefaultPawnClassForController(new_player)->GetDefaultObject());
	if (!spawn || !character)
	{
		return Super::SpawnDefaultPawnFor_Implementation(new_player, start_spot);
	}
	// 좌표는 에디터에서 지형 충돌로 구한 값이라 실행 중 높이와 조금 다를 수 있다.
	// 캡슐을 위에서 떨어뜨려 실제로 닿는 높이에 생성해 스폰할 때 땅에 박히지 않게 한다.
	const UCapsuleComponent* capsule = character->GetCapsuleComponent();
	const FCollisionShape shape = FCollisionShape::MakeCapsule(capsule->GetScaledCapsuleRadius(), capsule->GetScaledCapsuleHalfHeight());
	FTransform transform = *spawn;
	const FVector location = transform.GetLocation();
	FHitResult hit;
	if (GetWorld()->SweepSingleByChannel(hit, location + FVector(0.0, 0.0, 300.0), location - FVector(0.0, 0.0, 1000.0), FQuat::Identity, ECC_Pawn, shape, FCollisionQueryParams(SCENE_QUERY_STAT(StartDomeSpawn), false)) && !hit.bStartPenetrating)
	{
		transform.SetLocation(hit.Location + FVector(0.0, 0.0, 2.0));
	}
	return SpawnDefaultPawnAtTransform(new_player, transform);
}

TOptional<FTransform> AThatsMineGameMode::GetStartDomeSpawn() const
{
	// 시작 돔이 있는 게임 레벨에서만 쓴다. 온라인은 서버가 보낸 좌표를, 로컬 테스트는 첫 자리를 쓴다.
	if (!TActorIterator<AStartDome>(GetWorld()))
	{
		return {};
	}
	const UGameInstance* game_instance = GetGameInstance();
	if (!game_instance || game_instance->GetSubsystem<UClientStateSubsystem>()->IsLocalMode())
	{
		const SpawnPoint& point = START_DOME_SPAWN_POINTS[0];
		return FTransform(FRotator(0.0, point.yaw, 0.0), FVector(point.x, point.y, point.z));
	}
	return game_instance->GetSubsystem<UOnlineSessionSubsystem>()->GetSpawnTransform();
}
