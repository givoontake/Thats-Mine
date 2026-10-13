#include "HintDropSubsystem.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Flow/ClientStateSubsystem.h"
#include "Network/GameNetworkSubsystem.h"
#include "Network/game_protocol.h"
#include "settings.h"

DEFINE_LOG_CATEGORY_STATIC(LogHintDrop, Log, All);

static constexpr TCHAR SCROLL_MESH_PATH[] = TEXT("/Game/Thats_Mine/Meshes/Props/Scrolls/SM_Scroll_Rolled.SM_Scroll_Rolled");
// 바닥을 찾을 때 서버가 보낸 위치에서 아래로 내려 보는 거리(cm)
static constexpr double SCROLL_GROUND_TRACE_DISTANCE = 100000.0;
// 등급별 밴드 색. 두루마리 재질은 밴드가 쓰는 색 견본 칸(4행 1열)의 UV만 앞의 두 값만큼 옮긴다. S, A, B, C 순서다.
// 세 번째 값이 1이면 옮긴 칸의 위에서 아래까지 색을 두루마리 앞뒤 방향으로 펼쳐 그라데이션으로 보인다.
// 네 번째 값이 0보다 크면 옮긴 칸의 위에서 그 비율 지점 색 하나로 밴드를 칠한다. S·A는 그라데이션이다.
static constexpr float SCROLL_BAND_DATA[][4] = { { 0.875f, -0.5f, 1.0f, 0.0f }, { 0.0f, -0.5f, 1.0f, 0.0f }, { 0.375f, 0.0f, 0.0f, 0.0f }, { 0.25f, -0.75f, 0.0f, 0.0f } };

void UHintDropSubsystem::OnWorldBeginPlay(UWorld& world)
{
	Super::OnWorldBeginPlay(world);
	UGameInstance* game_instance = world.GetGameInstance();
	const UClientStateSubsystem* flow = game_instance ? game_instance->GetSubsystem<UClientStateSubsystem>() : nullptr;
	// 온라인 게임 레벨은 로딩 상태에서 열린다.
	if (!flow || flow->IsLocalMode() || flow->GetState() != EClientState::LOADING)
	{
		return;
	}
	UGameNetworkSubsystem* network = game_instance->GetSubsystem<UGameNetworkSubsystem>();
	is_active_ = network->RegisterPacketHandler(S2C_HINT_DROP, FNetworkPacketHandler::CreateUObject(this, &UHintDropSubsystem::HandleHintDropPacket))
		&& network->RegisterPacketHandler(S2C_HINT_PICKUP, FNetworkPacketHandler::CreateUObject(this, &UHintDropSubsystem::HandleHintPickupPacket));
}

void UHintDropSubsystem::Deinitialize()
{
	if (is_active_)
	{
		// 게임이 종료될 때는 게임 인스턴스 서브시스템이 월드보다 먼저 정리되어 없을 수 있다.
		UGameInstance* game_instance = GetWorld()->GetGameInstance();
		if (UGameNetworkSubsystem* network = game_instance ? game_instance->GetSubsystem<UGameNetworkSubsystem>() : nullptr)
		{
			network->UnregisterPacketHandler(S2C_HINT_DROP);
			network->UnregisterPacketHandler(S2C_HINT_PICKUP);
		}
		is_active_ = false;
	}
	Super::Deinitialize();
}

bool UHintDropSubsystem::DoesSupportWorldType(const EWorldType::Type world_type) const
{
	return world_type == EWorldType::Game || world_type == EWorldType::PIE;
}

bool UHintDropSubsystem::RequestPickup(const FVector& capsule_location) const
{
	const int32 nearest = FindPickupDrop(capsule_location);
	if (nearest == INDEX_NONE)
	{
		return false;
	}
	C2S_HINT_PICKUP_PACKET packet{};
	packet.drop = static_cast<uint16>(nearest);
	TArray<uint8> payload;
	payload.Append(reinterpret_cast<const uint8*>(&packet) + sizeof(PACKET_HEADER), sizeof(packet) - sizeof(PACKET_HEADER));
	GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>()->SendPacket(C2S_HINT_PICKUP, payload);
	return true;
}

int32 UHintDropSubsystem::FindPickupDrop(const FVector& capsule_location) const
{
	if (!is_active_)
	{
		return INDEX_NONE;
	}
	int32 nearest = INDEX_NONE;
	double nearest_distance = HINT_DROP_PICKUP_DISTANCE;
	for (const auto& [drop, location] : drop_locations_)
	{
		const double distance = FVector::Dist2D(capsule_location, location);
		if (distance <= nearest_distance && capsule_location.Z <= location.Z + HINT_DROP_PICKUP_HEIGHT)
		{
			nearest = drop;
			nearest_distance = distance;
		}
	}
	return nearest;
}

// 떨어진 힌트. 플레이 중인 방 전체가 받고, 게임 도중 들어오면 아직 줍지 않은 것을 받는다.
// 서버 위치에서 아래로 내려 처음 닿는 고정 물체(땅·건물) 위에 눕힌다. 방향은 드랍 번호로 정해 모든 화면에서 같고, 밴드 색은 힌트 등급을 따른다.
void UHintDropSubsystem::HandleHintDropPacket(const TArray<uint8>& payload)
{
	S2C_HINT_DROP_PACKET packet;
	if (payload.Num() != sizeof(packet) - sizeof(PACKET_HEADER))
	{
		UE_LOG(LogHintDrop, Warning, TEXT("Invalid hint drop packet."));
		GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>()->Disconnect();
		return;
	}
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	UStaticMesh* mesh = LoadObject<UStaticMesh>(nullptr, SCROLL_MESH_PATH);
	if (!mesh)
	{
		UE_LOG(LogHintDrop, Error, TEXT("Scroll mesh not found."));
		return;
	}
	const FVector location(packet.location[0], packet.location[1], packet.location[2]);
	FHitResult hit;
	const FVector ground = GetWorld()->LineTraceSingleByObjectType(hit, location, location - FVector(0.0, 0.0, SCROLL_GROUND_TRACE_DISTANCE), FCollisionObjectQueryParams(ECC_WorldStatic))
		? hit.ImpactPoint
		: location - FVector(0.0, 0.0, CHARACTER_CAPSULE_HALF_HEIGHT);
	const FBoxSphereBounds bounds = mesh->GetBounds();
	const FTransform transform(FRotator(0.0, packet.drop * 137.0, 0.0), ground + FVector(0.0, 0.0, bounds.BoxExtent.Z - bounds.Origin.Z));
	AStaticMeshActor* scroll = GetWorld()->SpawnActorDeferred<AStaticMeshActor>(AStaticMeshActor::StaticClass(), transform);
	scroll->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
	scroll->GetStaticMeshComponent()->SetStaticMesh(mesh);
	if (packet.grade < UE_ARRAY_COUNT(SCROLL_BAND_DATA))
	{
		for (int32 index = 0; index < UE_ARRAY_COUNT(SCROLL_BAND_DATA[0]); ++index)
		{
			scroll->GetStaticMeshComponent()->SetCustomPrimitiveDataFloat(index, SCROLL_BAND_DATA[packet.grade][index]);
		}
	}
	// 줍기는 거리로 정하므로 충돌은 없다. 밟고 지나가도 걸리지 않는다.
	scroll->SetActorEnableCollision(false);
	scroll->FinishSpawning(transform);
	scrolls_.Add(packet.drop, scroll);
	drop_locations_.Add(packet.drop, location);
}

// 서버가 인정한 줍기. 플레이 중인 방 전체가 받는다. 주운 사람에게는 이어서 힌트가 온다.
void UHintDropSubsystem::HandleHintPickupPacket(const TArray<uint8>& payload)
{
	S2C_HINT_PICKUP_PACKET packet;
	if (payload.Num() != sizeof(packet) - sizeof(PACKET_HEADER))
	{
		UE_LOG(LogHintDrop, Warning, TEXT("Invalid hint pickup packet."));
		GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>()->Disconnect();
		return;
	}
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	TObjectPtr<AStaticMeshActor> scroll;
	if (scrolls_.RemoveAndCopyValue(packet.drop, scroll) && IsValid(scroll))
	{
		scroll->Destroy();
	}
	drop_locations_.Remove(packet.drop);
}
