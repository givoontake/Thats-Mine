#include "HintSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
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
// 영역 지우기 안내를 띄우는 남은 시간(초)
static constexpr int32 AREA_REMOVAL_NOTICE_SECONDS[] = { 60, 30, 10 };

void UHintSubsystem::OnWorldBeginPlay(UWorld& world)
{
	Super::OnWorldBeginPlay(world);
	// 문장은 핑의 물체 종류 이름에도 쓰므로 로컬 플레이에서도 읽는다.
	LoadTexts();
	UGameInstance* game_instance = world.GetGameInstance();
	const UClientStateSubsystem* flow = game_instance ? game_instance->GetSubsystem<UClientStateSubsystem>() : nullptr;
	// 온라인 게임 레벨은 로딩 상태에서 열린다.
	if (!flow || flow->IsLocalMode() || flow->GetState() != EClientState::LOADING)
	{
		return;
	}
	UGameNetworkSubsystem* network = game_instance->GetSubsystem<UGameNetworkSubsystem>();
	is_active_ = network->RegisterPacketHandler(S2C_DIRECTION_HINT, FNetworkPacketHandler::CreateUObject(this, &UHintSubsystem::HandleDirectionHintPacket))
		&& network->RegisterPacketHandler(S2C_COUNT_HINT, FNetworkPacketHandler::CreateUObject(this, &UHintSubsystem::HandleCountHintPacket))
		&& network->RegisterPacketHandler(S2C_HINT_LOST, FNetworkPacketHandler::CreateUObject(this, &UHintSubsystem::HandleHintLostPacket))
		&& network->RegisterPacketHandler(S2C_EXCLUSION_BOARD, FNetworkPacketHandler::CreateUObject(this, &UHintSubsystem::HandleExclusionBoardPacket))
		&& network->RegisterPacketHandler(S2C_AREA_REMOVAL_TIME, FNetworkPacketHandler::CreateUObject(this, &UHintSubsystem::HandleAreaRemovalTimePacket))
		&& network->RegisterPacketHandler(S2C_AREA_REMOVAL, FNetworkPacketHandler::CreateUObject(this, &UHintSubsystem::HandleAreaRemovalPacket));
}

void UHintSubsystem::Deinitialize()
{
	if (is_active_)
	{
		// 게임이 종료될 때는 게임 인스턴스 서브시스템이 월드보다 먼저 정리되어 없을 수 있다.
		UGameInstance* game_instance = GetWorld()->GetGameInstance();
		if (UGameNetworkSubsystem* network = game_instance ? game_instance->GetSubsystem<UGameNetworkSubsystem>() : nullptr)
		{
			network->UnregisterPacketHandler(S2C_DIRECTION_HINT);
			network->UnregisterPacketHandler(S2C_COUNT_HINT);
			network->UnregisterPacketHandler(S2C_HINT_LOST);
			network->UnregisterPacketHandler(S2C_EXCLUSION_BOARD);
			network->UnregisterPacketHandler(S2C_AREA_REMOVAL_TIME);
			network->UnregisterPacketHandler(S2C_AREA_REMOVAL);
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

// 내가 얻은 방향 힌트. 상자 열림이나 기절 적용 뒤에 이어 온다. 문장 틀에 영역 이름(REGION_번호)과 방위 이름(DIRECTION_번호)을 채운다.
void UHintSubsystem::HandleDirectionHintPacket(const TArray<uint8>& payload)
{
	S2C_DIRECTION_HINT_PACKET packet;
	if (payload.Num() != sizeof(packet) - sizeof(PACKET_HEADER))
	{
		UE_LOG(LogHint, Warning, TEXT("Invalid direction hint packet."));
		GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>()->Disconnect();
		return;
	}
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	APlayerCharacter* character = Cast<APlayerCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	if (!character)
	{
		return;
	}
	FInventoryHint hint;
	hint.hint_id = packet.hint_id;
	hint.category = EHintCategory::LOCATION;
	hint.text = FText::FromString(FString::Format(*FindText(packet.is_true ? TEXT("DIRECTION_TRUE") : TEXT("DIRECTION_FALSE")),
		{ FindText(TEXT("REGION_") + FString::FromInt(packet.area)), FindText(TEXT("DIRECTION_") + FString::FromInt(packet.direction)) }));
	character->AddHint(hint);
}

// 내가 얻은 개수 힌트. 상자 열림이나 기절 적용 뒤에 이어 온다. 문장 틀에 반경(m), 물체 종류 이름(OBJECT_번호), 개수를 채운다.
void UHintSubsystem::HandleCountHintPacket(const TArray<uint8>& payload)
{
	S2C_COUNT_HINT_PACKET packet;
	if (payload.Num() != sizeof(packet) - sizeof(PACKET_HEADER))
	{
		UE_LOG(LogHint, Warning, TEXT("Invalid count hint packet."));
		GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>()->Disconnect();
		return;
	}
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	APlayerCharacter* character = Cast<APlayerCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	if (!character || packet.radius >= UE_ARRAY_COUNT(HINT_OBJECT_COUNT_RADII))
	{
		return;
	}
	const FString radius = FString::FromInt(FMath::RoundToInt32(HINT_OBJECT_COUNT_RADII[packet.radius] / 100.0f));
	const FString name = FindText(TEXT("OBJECT_") + FString::FromInt(packet.object_kind));
	FInventoryHint hint;
	hint.hint_id = packet.hint_id;
	hint.category = EHintCategory::CLUE;
	hint.object_kind = packet.object_kind;
	hint.radius = packet.radius;
	hint.count = packet.count;
	if (packet.count > 0)
	{
		hint.text = FText::FromString(FString::Format(*FindText(TEXT("OBJECT_COUNT")), { radius, name, FString::FromInt(packet.count) }));
	}
	else
	{
		// 0개 문장의 조사는 이름 마지막 글자에 받침이 있으면 "이", 없으면 "가"다.
		const TCHAR last = name.IsEmpty() ? 0 : name[name.Len() - 1];
		const bool has_final = last >= 0xAC00 && last <= 0xD7A3 && (last - 0xAC00) % 28 != 0;
		hint.text = FText::FromString(FString::Format(*FindText(TEXT("OBJECT_COUNT_ZERO")), { radius, name, has_final ? TEXT("이") : TEXT("가") }));
	}
	character->AddHint(hint);
}

// 기절해서 빼앗긴 힌트의 번호. 기절 적용 뒤에 이어 온다. 그 번호의 힌트를 인벤토리에서 뺀다.
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
	if (APlayerCharacter* character = Cast<APlayerCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0)))
	{
		character->RemoveHint(packet.hint_id);
	}
}

// 배제 판이 바뀌었다. 배제 칸을 다시 칠한다.
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
	NotifyExclusionChanged();
}

void UHintSubsystem::RepaintExclusionMask()
{
	exclusion_mask_.Init(0, EXCLUSION_MASK_SIZE * EXCLUSION_MASK_SIZE);
	PaintExclusionBoard();
}

// 다음 영역 지우기까지 남은 시간. 안내할 남은 시간마다 아직 지나지 않은 것에 타이머를 건다.
void UHintSubsystem::HandleAreaRemovalTimePacket(const TArray<uint8>& payload)
{
	S2C_AREA_REMOVAL_TIME_PACKET packet;
	if (payload.Num() != sizeof(packet) - sizeof(PACKET_HEADER))
	{
		UE_LOG(LogHint, Warning, TEXT("Invalid area removal time packet."));
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
	for (const int32 seconds : AREA_REMOVAL_NOTICE_SECONDS)
	{
		if (remaining > seconds)
		{
			timers.SetTimer(notice_timers_.AddDefaulted_GetRef(), FTimerDelegate::CreateUObject(this, &UHintSubsystem::ShowAreaRemovalNotice, seconds), remaining - seconds, false);
		}
	}
}

// 영역 지우기 차례에 지운 칸. 바뀐 배제 판이 뒤에 이어 온다.
void UHintSubsystem::HandleAreaRemovalPacket(const TArray<uint8>& payload)
{
	S2C_AREA_REMOVAL_PACKET packet;
	if (payload.Num() != sizeof(packet) - sizeof(PACKET_HEADER))
	{
		UE_LOG(LogHint, Warning, TEXT("Invalid area removal packet."));
		GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>()->Disconnect();
		return;
	}
	ShowNotice(NSLOCTEXT("Hint", "AreaRemoved", "보물이 없는 지역이 공개되었습니다!"));
}

void UHintSubsystem::ShowAreaRemovalNotice(int32 seconds)
{
	ShowNotice(seconds >= 60
		? FText::Format(NSLOCTEXT("Hint", "AreaRemovalMinutes", "{0}분 후에 보물이 없는 지역이 공개됩니다!"), FText::AsNumber(seconds / 60))
		: FText::Format(NSLOCTEXT("Hint", "AreaRemovalSeconds", "{0}초 후에 보물이 없는 지역이 공개됩니다!"), FText::AsNumber(seconds)));
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

void UHintSubsystem::NotifyExclusionChanged()
{
	constexpr int32 size = EXCLUSION_MASK_SIZE;
	exclusion_texture_ = UTexture2D::CreateTransient(size, size, PF_B8G8R8A8);
	exclusion_texture_->Filter = TF_Bilinear;
	exclusion_texture_->SRGB = false;
	FTexture2DMipMap& mip = exclusion_texture_->GetPlatformData()->Mips[0];
	FColor* pixels = static_cast<FColor*>(mip.BulkData.Lock(LOCK_READ_WRITE));
	for (int32 index = 0; index < size * size; ++index)
	{
		pixels[index] = FColor(0, 0, 0, exclusion_mask_[index] ? 255 : 0);
	}
	mip.BulkData.Unlock();
	exclusion_texture_->UpdateResource();
	on_exclusion_mask_changed_.Broadcast();
}
