#include "NetworkWorldSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameNetworkSubsystem.h"

void UNetworkWorldSubsystem::Initialize(FSubsystemCollectionBase& collection)
{
	Super::Initialize(collection);
	tick_handle_ = FWorldDelegates::OnWorldTickStart.AddUObject(this, &UNetworkWorldSubsystem::OnWorldTickStart);
}

void UNetworkWorldSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldTickStart.Remove(tick_handle_);
	tick_handle_.Reset();
	Super::Deinitialize();
}

bool UNetworkWorldSubsystem::DoesSupportWorldType(const EWorldType::Type world_type) const
{
	return world_type == EWorldType::Game || world_type == EWorldType::PIE;
}

void UNetworkWorldSubsystem::OnWorldTickStart(UWorld* world, ELevelTick tick_type, float delta_seconds)
{
	if (world != GetWorld())
	{
		return;
	}
	UGameInstance* game_instance = world->GetGameInstance();
	// 맵 전환 중의 이전 월드나 다른 PIE 월드가 큐를 소비하지 않게 한다.
	if (!game_instance || game_instance->GetWorld() != world)
	{
		return;
	}
	if (UGameNetworkSubsystem* network = game_instance->GetSubsystem<UGameNetworkSubsystem>())
	{
		network->ProcessPackets();
	}
}
