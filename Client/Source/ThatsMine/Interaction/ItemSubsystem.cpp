#include "ItemSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Flow/ClientStateSubsystem.h"
#include "Interaction/HintSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Network/GameNetworkSubsystem.h"
#include "Network/game_protocol.h"
#include "Player/PlayerCharacter.h"

DEFINE_LOG_CATEGORY_STATIC(LogItem, Log, All);

void UItemSubsystem::OnWorldBeginPlay(UWorld& world)
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
	is_active_ = network->RegisterPacketHandler(S2C_ITEM, FNetworkPacketHandler::CreateUObject(this, &UItemSubsystem::HandleItemPacket))
		&& network->RegisterPacketHandler(S2C_SEARCH_RESULT, FNetworkPacketHandler::CreateUObject(this, &UItemSubsystem::HandleSearchResultPacket));
}

void UItemSubsystem::Deinitialize()
{
	if (is_active_)
	{
		// 게임이 종료될 때는 게임 인스턴스 서브시스템이 월드보다 먼저 정리되어 없을 수 있다.
		UGameInstance* game_instance = GetWorld()->GetGameInstance();
		if (UGameNetworkSubsystem* network = game_instance ? game_instance->GetSubsystem<UGameNetworkSubsystem>() : nullptr)
		{
			network->UnregisterPacketHandler(S2C_ITEM);
			network->UnregisterPacketHandler(S2C_SEARCH_RESULT);
		}
		is_active_ = false;
	}
	Super::Deinitialize();
}

bool UItemSubsystem::DoesSupportWorldType(const EWorldType::Type world_type) const
{
	return world_type == EWorldType::Game || world_type == EWorldType::PIE;
}

void UItemSubsystem::RequestSearch(TOptional<EHintGrade> grade) const
{
	if (!is_active_)
	{
		return;
	}
	C2S_SEARCH_PACKET packet{};
	packet.grade = grade ? static_cast<uint8>(*grade) : BASIC_MAGNIFIER_GRADE;
	TArray<uint8> payload;
	payload.Append(reinterpret_cast<const uint8*>(&packet) + sizeof(PACKET_HEADER), sizeof(packet) - sizeof(PACKET_HEADER));
	GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>()->SendPacket(C2S_SEARCH, payload);
}

// 내가 연 상자의 아이템. 상자 열림 패킷 뒤에 이어 온다.
void UItemSubsystem::HandleItemPacket(const TArray<uint8>& payload)
{
	S2C_ITEM_PACKET packet;
	if (payload.Num() != sizeof(packet) - sizeof(PACKET_HEADER))
	{
		UE_LOG(LogItem, Warning, TEXT("Invalid item packet."));
		GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>()->Disconnect();
		return;
	}
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	APlayerCharacter* character = Cast<APlayerCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	if (character && packet.kind < static_cast<uint8>(EItemKind::COUNT) && packet.grade <= static_cast<uint8>(EHintGrade::A))
	{
		character->AddItem(static_cast<EItemKind>(packet.kind), static_cast<EHintGrade>(packet.grade));
	}
}

// 내 돋보기 탐색 결과. 서버가 기본 돋보기의 재사용 대기시간을 시작했거나 넓은 탐지 돋보기를 하나 쓴 것이다.
void UItemSubsystem::HandleSearchResultPacket(const TArray<uint8>& payload)
{
	S2C_SEARCH_RESULT_PACKET packet;
	if (payload.Num() != sizeof(packet) - sizeof(PACKET_HEADER))
	{
		UE_LOG(LogItem, Warning, TEXT("Invalid search result packet."));
		GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>()->Disconnect();
		return;
	}
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	if (packet.grade != BASIC_MAGNIFIER_GRADE && packet.grade > static_cast<uint8>(EHintGrade::A))
	{
		return;
	}
	if (APlayerCharacter* character = Cast<APlayerCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0)))
	{
		character->ConsumeMagnifier(packet.grade == BASIC_MAGNIFIER_GRADE ? TOptional<EHintGrade>() : TOptional<EHintGrade>(static_cast<EHintGrade>(packet.grade)));
	}
	GetWorld()->GetSubsystem<UHintSubsystem>()->AddSearchResult(FVector2D(packet.location[0], packet.location[1]), packet.radius, packet.is_found);
}
