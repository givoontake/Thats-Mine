#include "HintSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Flow/ClientStateSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Network/GameNetworkSubsystem.h"
#include "Network/game_protocol.h"
#include "Player/PlayerCharacter.h"

DEFINE_LOG_CATEGORY_STATIC(LogHint, Log, All);

// 문장 파일. 패키징할 때 Content/Data 폴더를 함께 넣는다.
static const TCHAR* HINT_TEXT_PATH = TEXT("Data/hint_texts.txt");

void UHintSubsystem::OnWorldBeginPlay(UWorld& world)
{
	Super::OnWorldBeginPlay(world);
	UGameInstance* game_instance = world.GetGameInstance();
	const UClientStateSubsystem* flow = game_instance ? game_instance->GetSubsystem<UClientStateSubsystem>() : nullptr;
	// 온라인 게임 레벨은 로딩 상태에서 열린다.
	if (!flow || flow->IsLocalMode() || flow->GetState() != EClientState::LOADING)
	{
		return;
	}
	LoadTexts();
	is_active_ = game_instance->GetSubsystem<UGameNetworkSubsystem>()->RegisterPacketHandler(S2C_HINT, FNetworkPacketHandler::CreateUObject(this, &UHintSubsystem::HandleHintPacket));
}

void UHintSubsystem::Deinitialize()
{
	if (is_active_)
	{
		// 게임이 종료될 때는 게임 인스턴스 서브시스템이 월드보다 먼저 정리되어 없을 수 있다.
		UGameInstance* game_instance = GetWorld()->GetGameInstance();
		if (UGameNetworkSubsystem* network = game_instance ? game_instance->GetSubsystem<UGameNetworkSubsystem>() : nullptr)
		{
			network->UnregisterPacketHandler(S2C_HINT);
		}
		is_active_ = false;
	}
	Super::Deinitialize();
}

bool UHintSubsystem::DoesSupportWorldType(const EWorldType::Type world_type) const
{
	return world_type == EWorldType::Game || world_type == EWorldType::PIE;
}

// 줄: 키 = 문장. 빈 줄과 #으로 시작하는 줄은 건너뛴다.
void UHintSubsystem::LoadTexts()
{
	texts_.Reset();
	TArray<FString> lines;
	if (!FFileHelper::LoadFileToStringArray(lines, *(FPaths::ProjectContentDir() / HINT_TEXT_PATH)))
	{
		UE_LOG(LogHint, Error, TEXT("Hint text file not found."));
		return;
	}
	for (const FString& line : lines)
	{
		FString key;
		FString text;
		if (line.IsEmpty() || line.StartsWith(TEXT("#")) || !line.Split(TEXT("="), &key, &text))
		{
			continue;
		}
		texts_.Add(key.TrimStartAndEnd(), text.TrimStartAndEnd());
	}
}

FString UHintSubsystem::FindText(const FString& key) const
{
	const FString* text = texts_.Find(key);
	if (!text)
	{
		UE_LOG(LogHint, Warning, TEXT("Hint text key not found: %s"), *key);
		return key;
	}
	return *text;
}

// 힌트 종류의 문장 틀에 이름을 채운다. 이름 키는 종류 이름 뒤에 번호를 붙인 것이다(REGION_3, OBJECT_2 등).
FString UHintSubsystem::BuildSentence(const HINT_ENTRY& hint) const
{
	const FString target = FString::FromInt(hint.target);
	const FString detail = FString::FromInt(hint.detail);
	switch (static_cast<HintKind>(hint.kind))
	{
	case HintKind::DIRECTION:
		return FString::Format(*FindText(hint.is_true ? TEXT("DIRECTION_TRUE") : TEXT("DIRECTION_FALSE")), { FindText(TEXT("REGION_") + target), FindText(TEXT("DIRECTION_") + detail) });
	case HintKind::VIEW:
		return FString::Format(*FindText(hint.is_true ? TEXT("VIEW_TRUE") : TEXT("VIEW_FALSE")), { FindText(TEXT("REGION_") + target) });
	case HintKind::NEAREST_OBJECT:
		return FString::Format(*FindText(TEXT("NEAREST_OBJECT")), { FindText(TEXT("RANK_") + detail), FindText(TEXT("OBJECT_") + target) });
	case HintKind::NEAREST_TOWER:
		return FString::Format(*FindText(TEXT("NEAREST_TOWER")), { FindText(TEXT("TOWER_") + target) });
	default:
		return FString();
	}
}

// 내가 연 상자의 힌트. 상자 열림 패킷 뒤에 이어 온다.
void UHintSubsystem::HandleHintPacket(const TArray<uint8>& payload)
{
	S2C_HINT_PACKET packet;
	if (payload.Num() != sizeof(packet) - sizeof(PACKET_HEADER))
	{
		UE_LOG(LogHint, Warning, TEXT("Invalid hint packet."));
		GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>()->Disconnect();
		return;
	}
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	APlayerCharacter* character = Cast<APlayerCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	if (!character || packet.hint.grade > static_cast<uint8>(EHintGrade::C))
	{
		return;
	}
	FInventoryHint hint;
	hint.grade = static_cast<EHintGrade>(packet.hint.grade);
	hint.text = FText::FromString(BuildSentence(packet.hint));
	character->AddHint(hint);
}
