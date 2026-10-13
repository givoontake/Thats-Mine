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
	is_active_ = network->RegisterPacketHandler(S2C_BOX_LAYOUT, FNetworkPacketHandler::CreateUObject(this, &UBoxLayoutSubsystem::HandleBoxLayoutPacket))
		&& network->RegisterPacketHandler(S2C_BOX_OPEN, FNetworkPacketHandler::CreateUObject(this, &UBoxLayoutSubsystem::HandleBoxOpenPacket));
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
			network->UnregisterPacketHandler(S2C_BOX_OPEN);
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
	// 상자 수 뒤에 상자 항목들이 이어 붙어 있다. 상자가 많으면 이 패킷 여러 개로 나눠 오므로, 받은 순서대로 상자 번호를 이어 붙인다.
	const int32 entry_offset = sizeof(S2C_BOX_LAYOUT_PACKET) - sizeof(PACKET_HEADER);
	uint16 box_count = 0;
	if (payload.Num() >= entry_offset)
	{
		FMemory::Memcpy(&box_count, payload.GetData() + offsetof(S2C_BOX_LAYOUT_PACKET, box_count) - sizeof(PACKET_HEADER), sizeof(box_count));
	}
	if (payload.Num() < entry_offset || payload.Num() != entry_offset + box_count * static_cast<int32>(sizeof(BOX_ENTRY)))
	{
		UE_LOG(LogBoxLayout, Warning, TEXT("Invalid box layout packet."));
		GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>()->Disconnect();
		return;
	}
	UStaticMesh* chest_mesh = LoadObject<UStaticMesh>(nullptr, CHEST_MESH_PATH);
	UStaticMesh* gift_mesh = LoadObject<UStaticMesh>(nullptr, GIFT_MESH_PATH);
	if (!chest_mesh || !gift_mesh)
	{
		UE_LOG(LogBoxLayout, Error, TEXT("Box mesh not found."));
		return;
	}
	for (int32 index = 0; index < box_count; ++index)
	{
		BOX_ENTRY entry;
		FMemory::Memcpy(&entry, payload.GetData() + entry_offset + index * sizeof(BOX_ENTRY), sizeof(entry));
		const bool is_gift = entry.kind == static_cast<uint8>(BoxKind::GIFT);
		// 이미 열린 선물은 사라진 상자라 만들지 않는다.
		if (is_gift && entry.is_opened)
		{
			boxes_.Add(nullptr);
			continue;
		}
		const FTransform transform(FRotator(entry.rotation[0], entry.rotation[1], entry.rotation[2]), FVector(entry.location[0], entry.location[1], entry.location[2]), FVector(is_gift ? 1.0f : CHEST_SCALE));
		AHintBox* box = SpawnMeshActor<AHintBox>(GetWorld(), is_gift ? gift_mesh : chest_mesh, transform);
		box->Tags = { TEXT("HintBox"), is_gift ? TEXT("HintGift") : TEXT("HintTreasure") };
		if (entry.is_opened)
		{
			box->ShowOpened();
		}
		boxes_.Add(box);
	}
}

void UBoxLayoutSubsystem::RequestOpen(const AHintBox* box, bool is_shot, uint8 weapon) const
{
	const int32 box_index = boxes_.IndexOfByKey(box);
	if (!is_active_ || box_index == INDEX_NONE || box->IsActivated() || (is_shot && !box->ActorHasTag(TEXT("HintGift"))))
	{
		return;
	}
	C2S_BOX_OPEN_PACKET packet{};
	packet.box = static_cast<uint16>(box_index);
	packet.is_shot = is_shot;
	packet.weapon = weapon;
	TArray<uint8> payload;
	payload.Append(reinterpret_cast<const uint8*>(&packet) + sizeof(PACKET_HEADER), sizeof(packet) - sizeof(PACKET_HEADER));
	GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>()->SendPacket(C2S_BOX_OPEN, payload);
}

// 서버가 인정한 상자 열림. 플레이 중인 방 전체가 받는다.
void UBoxLayoutSubsystem::HandleBoxOpenPacket(const TArray<uint8>& payload)
{
	S2C_BOX_OPEN_PACKET packet;
	if (payload.Num() != sizeof(packet) - sizeof(PACKET_HEADER))
	{
		UE_LOG(LogBoxLayout, Warning, TEXT("Invalid box open packet."));
		GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>()->Disconnect();
		return;
	}
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	if (boxes_.IsValidIndex(packet.box) && boxes_[packet.box])
	{
		boxes_[packet.box]->Open();
	}
}
