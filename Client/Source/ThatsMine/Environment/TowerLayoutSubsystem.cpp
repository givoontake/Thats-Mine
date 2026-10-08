#include "TowerLayoutSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Environment/Tower.h"
#include "Flow/ClientStateSubsystem.h"
#include "Network/GameNetworkSubsystem.h"
#include "Network/game_protocol.h"

DEFINE_LOG_CATEGORY_STATIC(LogTowerLayout, Log, All);

void UTowerLayoutSubsystem::OnWorldBeginPlay(UWorld& world)
{
	Super::OnWorldBeginPlay(world);
	UGameInstance* game_instance = world.GetGameInstance();
	const UClientStateSubsystem* flow = game_instance ? game_instance->GetSubsystem<UClientStateSubsystem>() : nullptr;
	// 온라인 게임 레벨은 로딩 상태에서 열려 서버의 배치를 기다린다.
	if (!flow || flow->IsLocalMode() || flow->GetState() != EClientState::LOADING)
	{
		return;
	}
	is_active_ = game_instance->GetSubsystem<UGameNetworkSubsystem>()->RegisterPacketHandler(S2C_TOWER_LAYOUT, FNetworkPacketHandler::CreateUObject(this, &UTowerLayoutSubsystem::HandleTowerLayoutPacket));
}

void UTowerLayoutSubsystem::Deinitialize()
{
	if (is_active_)
	{
		// 게임이 종료될 때는 게임 인스턴스 서브시스템이 월드보다 먼저 정리되어 없을 수 있다.
		UGameInstance* game_instance = GetWorld()->GetGameInstance();
		if (UGameNetworkSubsystem* network = game_instance ? game_instance->GetSubsystem<UGameNetworkSubsystem>() : nullptr)
		{
			network->UnregisterPacketHandler(S2C_TOWER_LAYOUT);
		}
		is_active_ = false;
	}
	Super::Deinitialize();
}

bool UTowerLayoutSubsystem::DoesSupportWorldType(const EWorldType::Type world_type) const
{
	return world_type == EWorldType::Game || world_type == EWorldType::PIE;
}

void UTowerLayoutSubsystem::HandleTowerLayoutPacket(const TArray<uint8>& payload)
{
	S2C_TOWER_LAYOUT_PACKET packet;
	const int32 location_offset = offsetof(S2C_TOWER_LAYOUT_PACKET, locations) - sizeof(PACKET_HEADER);
	if (payload.Num() < location_offset || payload[0] > TOWER_COUNT || payload.Num() != location_offset + payload[0] * static_cast<int32>(sizeof(packet.locations[0])))
	{
		UE_LOG(LogTowerLayout, Warning, TEXT("Invalid tower layout packet."));
		GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>()->Disconnect();
		return;
	}
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	for (int32 index = 0; index < packet.tower_count; ++index)
	{
		const float* location = packet.locations[index];
		ATower* tower = GetWorld()->SpawnActor<ATower>(FVector(location[0], location[1], location[2]), FRotator::ZeroRotator);
		if (tower)
		{
			tower->Tags = { TEXT("Tower") };
		}
	}
}
