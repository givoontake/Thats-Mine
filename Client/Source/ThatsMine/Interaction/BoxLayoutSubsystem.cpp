#include "BoxLayoutSubsystem.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Flow/ClientStateSubsystem.h"
#include "Interaction/HintBox.h"
#include "Network/GameNetworkSubsystem.h"
#include "Network/game_protocol.h"

DEFINE_LOG_CATEGORY_STATIC(LogBoxLayout, Log, All);

// 상자 후보를 만들 때 쓴 모델과 크기. 보물 상자는 0.7배, 선물 상자는 원래 크기다.
static constexpr TCHAR CHEST_MESH_PATH[] = TEXT("/Game/Thats_Mine/Meshes/Props/Chests/SM_Chest_Closed.SM_Chest_Closed");
static constexpr TCHAR GIFT_MESH_PATH[] = TEXT("/Game/Thats_Mine/Meshes/Props/Gifts/SM_Gift_D_Red.SM_Gift_D_Red");
static constexpr float CHEST_SCALE = 0.7f;

// 움직일 수 있는 정적 메시 액터를 만든다. 실행 중에 메시를 바꾸려면 등록 전에 이동 가능으로 바꿔 둬야 한다.
template <typename T>
static T* SpawnMeshActor(UWorld* world, UStaticMesh* mesh, const FTransform& transform)
{
	T* actor = world->SpawnActorDeferred<T>(T::StaticClass(), transform);
	actor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	actor->GetStaticMeshComponent()->SetStaticMesh(mesh);
	actor->FinishSpawning(transform);
	return actor;
}

void UBoxLayoutSubsystem::OnWorldBeginPlay(UWorld& world)
{
	Super::OnWorldBeginPlay(world);
	UGameInstance* game_instance = world.GetGameInstance();
	const UClientStateSubsystem* flow = game_instance ? game_instance->GetSubsystem<UClientStateSubsystem>() : nullptr;
	// 온라인 게임 레벨은 로딩 상태에서 열려 서버의 배치를 기다린다.
	if (!flow || flow->IsLocalMode() || flow->GetState() != EClientState::LOADING)
	{
		return;
	}
	UGameNetworkSubsystem* network = game_instance->GetSubsystem<UGameNetworkSubsystem>();
	is_active_ = network->RegisterPacketHandler(S2C_BOX_LAYOUT, FNetworkPacketHandler::CreateUObject(this, &UBoxLayoutSubsystem::HandleBoxLayoutPacket));
}

void UBoxLayoutSubsystem::Deinitialize()
{
	if (is_active_)
	{
		// 게임이 종료될 때는 게임 인스턴스 서브시스템이 월드보다 먼저 정리되어 없을 수 있다.
		UGameInstance* game_instance = GetWorld()->GetGameInstance();
		if (UGameNetworkSubsystem* network = game_instance ? game_instance->GetSubsystem<UGameNetworkSubsystem>() : nullptr)
		{
			network->UnregisterPacketHandler(S2C_BOX_LAYOUT);
		}
		is_active_ = false;
	}
	Super::Deinitialize();
}

bool UBoxLayoutSubsystem::DoesSupportWorldType(const EWorldType::Type world_type) const
{
	return world_type == EWorldType::Game || world_type == EWorldType::PIE;
}

void UBoxLayoutSubsystem::HandleBoxLayoutPacket(const TArray<uint8>& payload)
{
	S2C_BOX_LAYOUT_PACKET packet;
	const int32 entry_offset = offsetof(S2C_BOX_LAYOUT_PACKET, entries) - sizeof(PACKET_HEADER);
	if (payload.Num() < entry_offset || payload[0] > MAX_BOX_COUNT || payload.Num() != entry_offset + payload[0] * static_cast<int32>(sizeof(BOX_ENTRY)))
	{
		UE_LOG(LogBoxLayout, Warning, TEXT("Invalid box layout packet."));
		GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>()->Disconnect();
		return;
	}
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	UStaticMesh* chest_mesh = LoadObject<UStaticMesh>(nullptr, CHEST_MESH_PATH);
	UStaticMesh* gift_mesh = LoadObject<UStaticMesh>(nullptr, GIFT_MESH_PATH);
	if (!chest_mesh || !gift_mesh)
	{
		UE_LOG(LogBoxLayout, Error, TEXT("Box mesh not found."));
		return;
	}
	for (int32 index = 0; index < packet.box_count; ++index)
	{
		const BOX_ENTRY& entry = packet.entries[index];
		const bool is_gift = entry.kind == static_cast<uint8>(BoxKind::GIFT);
		const FTransform transform(FRotator(entry.rotation[0], entry.rotation[1], entry.rotation[2]), FVector(entry.location[0], entry.location[1], entry.location[2]), FVector(is_gift ? 1.0f : CHEST_SCALE));
		AHintBox* box = SpawnMeshActor<AHintBox>(GetWorld(), is_gift ? gift_mesh : chest_mesh, transform);
		box->Tags = { TEXT("HintBox"), is_gift ? TEXT("HintGift") : TEXT("HintTreasure") };
	}
}
