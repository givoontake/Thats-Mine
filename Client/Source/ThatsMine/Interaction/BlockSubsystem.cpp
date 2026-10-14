#include "BlockSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Flow/ClientStateSubsystem.h"
#include "Interaction/HintSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Network/GameNetworkSubsystem.h"
#include "Network/game_protocol.h"
#include "Player/PlayerCharacter.h"
#include "block_pieces.h"

DEFINE_LOG_CATEGORY_STATIC(LogBlock, Log, All);

void UBlockSubsystem::OnWorldBeginPlay(UWorld& world)
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
	is_active_ = network->RegisterPacketHandler(S2C_BLOCK, FNetworkPacketHandler::CreateUObject(this, &UBlockSubsystem::HandleBlockPacket))
		&& network->RegisterPacketHandler(S2C_BLOCK_RESULT, FNetworkPacketHandler::CreateUObject(this, &UBlockSubsystem::HandleBlockResultPacket));
}

void UBlockSubsystem::Deinitialize()
{
	if (is_active_)
	{
		// 게임이 종료될 때는 게임 인스턴스 서브시스템이 월드보다 먼저 정리되어 없을 수 있다.
		UGameInstance* game_instance = GetWorld()->GetGameInstance();
		if (UGameNetworkSubsystem* network = game_instance ? game_instance->GetSubsystem<UGameNetworkSubsystem>() : nullptr)
		{
			network->UnregisterPacketHandler(S2C_BLOCK);
			network->UnregisterPacketHandler(S2C_BLOCK_RESULT);
		}
		is_active_ = false;
	}
	Super::Deinitialize();
}

bool UBlockSubsystem::DoesSupportWorldType(const EWorldType::Type world_type) const
{
	return world_type == EWorldType::Game || world_type == EWorldType::PIE;
}

TArray<int32> UBlockSubsystem::GetUsableBlocks() const
{
	TArray<int32> blocks;
	for (int32 piece = 0; piece < BLOCK_PIECE_COUNT; ++piece)
	{
		if (block_counts_[piece] > 0)
		{
			blocks.Add(piece);
		}
	}
	return blocks;
}

void UBlockSubsystem::RequestUse(int32 piece, int32 rotation, int32 column, int32 row) const
{
	if (!is_active_)
	{
		return;
	}
	C2S_BLOCK_USE_PACKET packet{};
	packet.piece = static_cast<uint8>(piece);
	packet.rotation = static_cast<uint8>(rotation);
	packet.column = static_cast<uint8>(column);
	packet.row = static_cast<uint8>(row);
	TArray<uint8> payload;
	payload.Append(reinterpret_cast<const uint8*>(&packet) + sizeof(PACKET_HEADER), sizeof(packet) - sizeof(PACKET_HEADER));
	GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>()->SendPacket(C2S_BLOCK_USE, payload);
}

// 내가 연 상자의 블록. 상자 열림 패킷 뒤에 이어 온다. 이미 가진 블록이면 하나 더 가진다.
void UBlockSubsystem::HandleBlockPacket(const TArray<uint8>& payload)
{
	S2C_BLOCK_PACKET packet;
	if (payload.Num() != sizeof(packet) - sizeof(PACKET_HEADER))
	{
		UE_LOG(LogBlock, Warning, TEXT("Invalid block packet."));
		GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>()->Disconnect();
		return;
	}
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	if (packet.piece >= BLOCK_PIECE_COUNT)
	{
		return;
	}
	if (block_counts_[packet.piece] < MAX_uint8)
	{
		++block_counts_[packet.piece];
	}
	on_blocks_changed_.Broadcast();
	if (APlayerCharacter* character = Cast<APlayerCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0)))
	{
		FInventoryHint entry;
		entry.text = FText::Format(NSLOCTEXT("Block", "BlockText", "{0}칸 블록 · 블록 탭에서 지도에 놓아 쓸 수 있습니다"), FText::AsNumber(BLOCK_PIECES[packet.piece].cell_count));
		character->ShowAcquirePopup(entry, NSLOCTEXT("Block", "NewBlock", "블록 획득!"), false);
	}
}

// 서버가 인정한 내 블록 사용. 바뀐 배제 판은 뒤에 이어 와서 힌트 서브시스템이 받는다.
void UBlockSubsystem::HandleBlockResultPacket(const TArray<uint8>& payload)
{
	S2C_BLOCK_RESULT_PACKET packet;
	if (payload.Num() != sizeof(packet) - sizeof(PACKET_HEADER))
	{
		UE_LOG(LogBlock, Warning, TEXT("Invalid block result packet."));
		GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>()->Disconnect();
		return;
	}
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	if (packet.piece >= BLOCK_PIECE_COUNT)
	{
		return;
	}
	if (block_counts_[packet.piece] > 0)
	{
		--block_counts_[packet.piece];
	}
	on_blocks_changed_.Broadcast();
	GetWorld()->GetSubsystem<UHintSubsystem>()->ShowNotice(packet.is_inside
		? NSLOCTEXT("Block", "Inside", "블록 안에 보물이 있습니다!")
		: NSLOCTEXT("Block", "Outside", "블록 안에 보물이 없습니다.."));
}
