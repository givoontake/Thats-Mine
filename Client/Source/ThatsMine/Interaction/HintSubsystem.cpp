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
#include "TimerManager.h"
#include "UI/Hint/HintNoticeWidget.h"
#include "block_pieces.h"

DEFINE_LOG_CATEGORY_STATIC(LogHint, Log, All);

// 문장 파일. 패키징할 때 Content/Data 폴더를 함께 넣는다.
static const TCHAR* HINT_TEXT_PATH = TEXT("Data/hint_texts.txt");
// 공개 힌트 안내를 띄우는 남은 시간(초)
static constexpr int32 PUBLIC_HINT_NOTICE_SECONDS[] = { 180, 60, 30, 10 };

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
	UGameNetworkSubsystem* network = game_instance->GetSubsystem<UGameNetworkSubsystem>();
	is_active_ = network->RegisterPacketHandler(S2C_HINT, FNetworkPacketHandler::CreateUObject(this, &UHintSubsystem::HandleHintPacket))
		&& network->RegisterPacketHandler(S2C_HINT_EXCLUSION, FNetworkPacketHandler::CreateUObject(this, &UHintSubsystem::HandleExclusionPacket))
		&& network->RegisterPacketHandler(S2C_HINT_LOST, FNetworkPacketHandler::CreateUObject(this, &UHintSubsystem::HandleHintLostPacket))
		&& network->RegisterPacketHandler(S2C_EXCLUSION_BOARD, FNetworkPacketHandler::CreateUObject(this, &UHintSubsystem::HandleExclusionBoardPacket))
		&& network->RegisterPacketHandler(S2C_PUBLIC_HINT_TIME, FNetworkPacketHandler::CreateUObject(this, &UHintSubsystem::HandlePublicHintTimePacket));
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
			network->UnregisterPacketHandler(S2C_HINT_EXCLUSION);
			network->UnregisterPacketHandler(S2C_HINT_LOST);
			network->UnregisterPacketHandler(S2C_EXCLUSION_BOARD);
			network->UnregisterPacketHandler(S2C_PUBLIC_HINT_TIME);
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
	case HintKind::OBJECT_COUNT:
	{
		const FString radius = FString::FromInt(FMath::RoundToInt32(HINT_OBJECT_COUNT_RADIUS / 100.0f));
		const FString name = FindText(TEXT("OBJECT_") + target);
		if (hint.detail > 0)
		{
			return FString::Format(*FindText(TEXT("OBJECT_COUNT")), { radius, name, detail });
		}
		// 0개 문장의 조사는 이름 마지막 글자에 받침이 있으면 "이", 없으면 "가"다.
		const TCHAR last = name.IsEmpty() ? 0 : name[name.Len() - 1];
		const bool has_final = last >= 0xAC00 && last <= 0xD7A3 && (last - 0xAC00) % 28 != 0;
		return FString::Format(*FindText(TEXT("OBJECT_COUNT_ZERO")), { radius, name, has_final ? TEXT("이") : TEXT("가") });
	}
	default:
		return FString();
	}
}

// 내가 연 상자나 주운 두루마리의 힌트. 상자 열림이나 줍기 패킷 뒤에 이어 온다.
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
	// 서버는 이미 가진 힌트도 다시 보낸다. 받은 힌트 목록에는 한 번만 둔다.
	exclusion_hint_index_ = received_hints_.IndexOfByPredicate([&packet](const FReceivedHint& received) { return FMemory::Memcmp(&received.hint, &packet.hint, sizeof(HINT_ENTRY)) == 0; });
	if (exclusion_hint_index_ == INDEX_NONE)
	{
		exclusion_hint_index_ = received_hints_.Add({ packet.hint });
	}
}

// 방향 힌트가 배제하는 영역. 칸 가운데가 경계선보다 배제 방위 쪽이면 그 칸을 배제한다. 북쪽은 x가 작은 쪽, 동쪽은 y가 작은 쪽이다.
void UHintSubsystem::HandleExclusionPacket(const TArray<uint8>& payload)
{
	S2C_HINT_EXCLUSION_PACKET packet;
	if (payload.Num() != sizeof(packet) - sizeof(PACKET_HEADER))
	{
		UE_LOG(LogHint, Warning, TEXT("Invalid hint exclusion packet."));
		GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>()->Disconnect();
		return;
	}
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	if (packet.direction >= static_cast<uint8>(HintDirection::COUNT))
	{
		return;
	}
	// 배제는 바로 앞에 받은 힌트의 것이다.
	if (received_hints_.IsValidIndex(exclusion_hint_index_))
	{
		FReceivedHint& received = received_hints_[exclusion_hint_index_];
		received.has_exclusion = true;
		received.direction = packet.direction;
		received.boundary = packet.boundary;
	}
	PaintExclusion(packet.direction, packet.boundary);
	on_exclusion_mask_changed_.Broadcast();
}

// 기절해서 떨어뜨린 힌트. 드랍 패킷 뒤에 이어 온다. 같은 힌트를 인벤토리에서 빼고, 배제가 있던 힌트면 남은 배제로 칸을 다시 칠한다.
// 남은 배제가 없어도 칸은 비우지 않고 모두 0으로 두어 지도가 배제 표시를 지우게 한다.
void UHintSubsystem::HandleHintLostPacket(const TArray<uint8>& payload)
{
	S2C_HINT_LOST_PACKET packet;
	if (payload.Num() != sizeof(packet) - sizeof(PACKET_HEADER))
	{
		UE_LOG(LogHint, Warning, TEXT("Invalid hint lost packet."));
		GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>()->Disconnect();
		return;
	}
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	const int32 index = received_hints_.IndexOfByPredicate([&packet](const FReceivedHint& received) { return FMemory::Memcmp(&received.hint, &packet.hint, sizeof(HINT_ENTRY)) == 0; });
	if (index == INDEX_NONE)
	{
		return;
	}
	const bool had_exclusion = received_hints_[index].has_exclusion;
	received_hints_.RemoveAt(index);
	exclusion_hint_index_ = INDEX_NONE;
	if (APlayerCharacter* character = Cast<APlayerCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0)))
	{
		character->RemoveHint(FText::FromString(BuildSentence(packet.hint)));
	}
	if (!had_exclusion)
	{
		return;
	}
	RepaintExclusionMask();
	on_exclusion_mask_changed_.Broadcast();
}

// 배제 판이 바뀌었다. 힌트를 잃어 줄어들 수도 있으므로 배제 칸을 다시 칠한다.
void UHintSubsystem::HandleExclusionBoardPacket(const TArray<uint8>& payload)
{
	S2C_EXCLUSION_BOARD_PACKET packet;
	if (payload.Num() != sizeof(packet) - sizeof(PACKET_HEADER))
	{
		UE_LOG(LogHint, Warning, TEXT("Invalid exclusion board packet."));
		GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>()->Disconnect();
		return;
	}
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	exclusion_board_ = packet.excluded_cells;
	RepaintExclusionMask();
	on_exclusion_mask_changed_.Broadcast();
}

void UHintSubsystem::RepaintExclusionMask()
{
	exclusion_mask_.Init(0, EXCLUSION_MASK_SIZE * EXCLUSION_MASK_SIZE);
	for (const FReceivedHint& received : received_hints_)
	{
		if (received.has_exclusion)
		{
			PaintExclusion(received.direction, received.boundary);
		}
	}
	for (const FSearchExclusion& search : search_exclusions_)
	{
		PaintSearchExclusion(search);
	}
	PaintExclusionBoard();
}

// 다음 공개 힌트까지 남은 시간. 안내할 남은 시간마다 아직 지나지 않은 것에 타이머를 건다.
void UHintSubsystem::HandlePublicHintTimePacket(const TArray<uint8>& payload)
{
	S2C_PUBLIC_HINT_TIME_PACKET packet;
	if (payload.Num() != sizeof(packet) - sizeof(PACKET_HEADER))
	{
		UE_LOG(LogHint, Warning, TEXT("Invalid public hint time packet."));
		GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>()->Disconnect();
		return;
	}
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	FTimerManager& timers = GetWorld()->GetTimerManager();
	for (FTimerHandle& timer : notice_timers_)
	{
		timers.ClearTimer(timer);
	}
	notice_timers_.Reset();
	const float remaining = packet.remaining_ms / 1000.0f;
	for (const int32 seconds : PUBLIC_HINT_NOTICE_SECONDS)
	{
		if (remaining > seconds)
		{
			timers.SetTimer(notice_timers_.AddDefaulted_GetRef(), FTimerDelegate::CreateUObject(this, &UHintSubsystem::ShowPublicHintNotice, seconds), remaining - seconds, false);
		}
	}
}

void UHintSubsystem::ShowPublicHintNotice(int32 seconds)
{
	ShowNotice(seconds >= 60
		? FText::Format(NSLOCTEXT("Hint", "PublicHintMinutes", "{0}분 후에 힌트가 제공됩니다!"), FText::AsNumber(seconds / 60))
		: FText::Format(NSLOCTEXT("Hint", "PublicHintSeconds", "{0}초 후에 힌트가 제공됩니다!"), FText::AsNumber(seconds)));
}

void UHintSubsystem::ShowNotice(const FText& text)
{
	APlayerController* controller = GetWorld()->GetFirstPlayerController();
	if (!notice_widget_ && controller && controller->IsLocalController())
	{
		notice_widget_ = CreateWidget<UHintNoticeWidget>(controller, UHintNoticeWidget::StaticClass());
		if (notice_widget_)
		{
			notice_widget_->AddToViewport(10);
		}
	}
	if (notice_widget_)
	{
		notice_widget_->ShowNotice(text);
	}
}

void UHintSubsystem::AddSearchResult(const FVector2D& center, float radius, bool is_found)
{
	const FSearchExclusion& search = search_exclusions_.Add_GetRef({ center, radius, is_found });
	PaintSearchExclusion(search);
	on_exclusion_mask_changed_.Broadcast();
	ShowNotice(is_found
		? FText::Format(NSLOCTEXT("Hint", "SearchFound", "{0}m 이내에 보물을 발견했습니다!"), FText::AsNumber(FMath::RoundToInt32(radius / 100.0f)))
		: NSLOCTEXT("Hint", "SearchNotFound", "발견하지 못했습니다.."));
}

// 칸 가운데가 탐색 원 안인지로 칠한다. 보물을 찾았으면 원 밖, 못 찾았으면 원 안의 칸을 배제한다.
void UHintSubsystem::PaintSearchExclusion(const FSearchExclusion& search)
{
	if (exclusion_mask_.IsEmpty())
	{
		exclusion_mask_.Init(0, EXCLUSION_MASK_SIZE * EXCLUSION_MASK_SIZE);
	}
	for (int32 row = 0; row < EXCLUSION_MASK_SIZE; ++row)
	{
		const double x = MAP_MIN[0] + (row + 0.5) / EXCLUSION_MASK_SIZE * (MAP_MAX[0] - MAP_MIN[0]);
		for (int32 column = 0; column < EXCLUSION_MASK_SIZE; ++column)
		{
			const double y = MAP_MAX[1] - (column + 0.5) / EXCLUSION_MASK_SIZE * (MAP_MAX[1] - MAP_MIN[1]);
			const bool is_inside = FVector2D::DistSquared(FVector2D(x, y), search.center) <= FMath::Square(static_cast<double>(search.radius));
			if (is_inside != search.is_found)
			{
				exclusion_mask_[row * EXCLUSION_MASK_SIZE + column] = 1;
			}
		}
	}
}

void UHintSubsystem::PaintExclusion(uint8 direction_value, float boundary)
{
	const HintDirection direction = static_cast<HintDirection>(direction_value);
	if (exclusion_mask_.IsEmpty())
	{
		exclusion_mask_.Init(0, EXCLUSION_MASK_SIZE * EXCLUSION_MASK_SIZE);
	}
	for (int32 row = 0; row < EXCLUSION_MASK_SIZE; ++row)
	{
		const double x = MAP_MIN[0] + (row + 0.5) / EXCLUSION_MASK_SIZE * (MAP_MAX[0] - MAP_MIN[0]);
		for (int32 column = 0; column < EXCLUSION_MASK_SIZE; ++column)
		{
			const double y = MAP_MAX[1] - (column + 0.5) / EXCLUSION_MASK_SIZE * (MAP_MAX[1] - MAP_MIN[1]);
			const bool is_excluded = direction == HintDirection::NORTH ? x < boundary
				: direction == HintDirection::SOUTH ? x > boundary
				: direction == HintDirection::EAST ? y < boundary
				: y > boundary;
			if (is_excluded)
			{
				exclusion_mask_[row * EXCLUSION_MASK_SIZE + column] = 1;
			}
		}
	}
}

// 배제 판의 칸 하나는 배제 칸 EXCLUSION_MASK_SIZE / BLOCK_GRID_SIZE개 너비다.
void UHintSubsystem::PaintExclusionBoard()
{
	static_assert(EXCLUSION_MASK_SIZE % BLOCK_GRID_SIZE == 0, "Exclusion mask must divide into board cells.");
	constexpr int32 cell_size = EXCLUSION_MASK_SIZE / BLOCK_GRID_SIZE;
	for (int32 row = 0; row < EXCLUSION_MASK_SIZE; ++row)
	{
		for (int32 column = 0; column < EXCLUSION_MASK_SIZE; ++column)
		{
			if ((exclusion_board_ >> ((row / cell_size) * BLOCK_GRID_SIZE + column / cell_size) & 1) != 0)
			{
				exclusion_mask_[row * EXCLUSION_MASK_SIZE + column] = 1;
			}
		}
	}
}
