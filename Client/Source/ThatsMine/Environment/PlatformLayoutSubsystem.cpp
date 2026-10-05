#include "PlatformLayoutSubsystem.h"

#include <random>
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Flow/ClientStateSubsystem.h"
#include "Network/GameNetworkSubsystem.h"
#include "Network/game_protocol.h"

DEFINE_LOG_CATEGORY_STATIC(LogPlatformLayout, Log, All);

void UPlatformLayoutSubsystem::OnWorldBeginPlay(UWorld& world)
{
	Super::OnWorldBeginPlay(world);
	UGameInstance* game_instance = world.GetGameInstance();
	const UClientStateSubsystem* flow = game_instance ? game_instance->GetSubsystem<UClientStateSubsystem>() : nullptr;
	if (!flow)
	{
		return;
	}
	// 로컬 플레이는 게임 레벨을 플레이 상태에서 열고, 온라인 게임 레벨은 로딩 상태에서 열어 서버의 배치를 기다린다.
	if (flow->IsLocalMode())
	{
		if (flow->GetState() == EClientState::PLAYING)
		{
			SpawnPlatforms(GeneratePlatformLayout(std::random_device{}()));
		}
		return;
	}
	if (flow->GetState() == EClientState::LOADING)
	{
		is_active_ = game_instance->GetSubsystem<UGameNetworkSubsystem>()->RegisterPacketHandler(S2C_PLATFORM_LAYOUT, FNetworkPacketHandler::CreateUObject(this, &UPlatformLayoutSubsystem::HandlePlatformLayoutPacket));
	}
}

void UPlatformLayoutSubsystem::Deinitialize()
{
	if (is_active_)
	{
		// 게임을 종료할 때는 게임 인스턴스의 서브시스템이 월드보다 먼저 정리되어 없을 수 있다.
		UGameInstance* game_instance = GetWorld()->GetGameInstance();
		if (UGameNetworkSubsystem* network = game_instance ? game_instance->GetSubsystem<UGameNetworkSubsystem>() : nullptr)
		{
			network->UnregisterPacketHandler(S2C_PLATFORM_LAYOUT);
		}
		is_active_ = false;
	}
	Super::Deinitialize();
}

bool UPlatformLayoutSubsystem::DoesSupportWorldType(const EWorldType::Type world_type) const
{
	return world_type == EWorldType::Game || world_type == EWorldType::PIE;
}

void UPlatformLayoutSubsystem::HandlePlatformLayoutPacket(const TArray<uint8>& payload)
{
	S2C_PLATFORM_LAYOUT_PACKET packet;
	if (payload.Num() != sizeof(packet) - sizeof(PACKET_HEADER))
	{
		UE_LOG(LogPlatformLayout, Warning, TEXT("Invalid platform layout packet."));
		GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>()->Disconnect();
		return;
	}
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	// 서버와 발판 자리 표가 다르면 같은 배치를 만들 수 없다.
	if (packet.platform_count != PLATFORM_COORDINATE_COUNT)
	{
		UE_LOG(LogPlatformLayout, Warning, TEXT("Platform count mismatch. server=%d client=%d"), packet.platform_count, PLATFORM_COORDINATE_COUNT);
		GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>()->Disconnect();
		return;
	}
	PlatformLayout layout{};
	for (int32 index = 0; index < PLATFORM_COORDINATE_COUNT; ++index)
	{
		layout[index] = (packet.spawned_bits[index / 8] >> (index % 8)) & 1;
	}
	SpawnPlatforms(layout);
}

void UPlatformLayoutSubsystem::SpawnPlatforms(const PlatformLayout& layout)
{
	UClass* platform_class = LoadClass<AActor>(nullptr, TEXT("/Game/Thats_Mine/Actors/Environments/LevelPrototyping/BP_InvisiblePlatform.BP_InvisiblePlatform_C"));
	if (!platform_class)
	{
		UE_LOG(LogPlatformLayout, Error, TEXT("Platform class not found."));
		return;
	}
	// 발판 메시는 한 변이 100cm인 정육면체라 발판 크기에 맞게 늘린다.
	// 블루프린트 기본값에도 크기가 들어 있고 생성할 때 넘긴 크기가 그 값에 곱해지므로, 기본 크기로 나눠서 넘긴다.
	const FVector default_scale = platform_class->GetDefaultObject<AActor>()->GetRootComponent()->GetRelativeScale3D();
	const FVector scale = FVector(PLATFORM_SIZE / 100.0f, PLATFORM_SIZE / 100.0f, PLATFORM_THICKNESS / 100.0f) / default_scale;
	for (int32 index = 0; index < PLATFORM_COORDINATE_COUNT; ++index)
	{
		if (!layout[index])
		{
			continue;
		}
		const PlatformCoordinate& coordinate = PLATFORM_COORDINATES[index];
		const FTransform transform(FRotator(0.0, coordinate.yaw, 0.0), FVector(coordinate.x, coordinate.y, coordinate.z), scale);
		GetWorld()->SpawnActor<AActor>(platform_class, transform);
	}
}
