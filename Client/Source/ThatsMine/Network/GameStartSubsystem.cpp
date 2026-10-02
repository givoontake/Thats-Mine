#include "GameStartSubsystem.h"

#include <cstddef>
#include "ContentStreaming.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Environment/StartDome.h"
#include "Flow/ClientStateSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "GameNetworkSubsystem.h"
#include "MovementSyncSubsystem.h"
#include "OnlineSessionSubsystem.h"
#include "Player/PlayerCharacter.h"
#include "ShaderPipelineCache.h"
#include "ThatsMineGameMode.h"
#include "UI/Loading/LoadingScreenSubsystem.h"
#include "UI/Timer/MatchTimerWidget.h"
#include "UObject/UObjectGlobals.h"
#include "game_protocol.h"

#define LOCTEXT_NAMESPACE "GameStart"

DEFINE_LOG_CATEGORY_STATIC(LogGameStart, Log, All);

// 맵이 열린 시점의 게이지 값. 나머지는 준비 항목이 끝날 때마다 채운다.
static constexpr float MAP_LOADED_PROGRESS = 0.8f;
// 준비가 끝나지 않아도 이 시간이 지나면 로딩 완료를 보낸다. 텍스처는 스트리밍 풀 상황에 따라 끝나지 않을 수 있다.
static constexpr double PREPARE_TIMEOUT_SECONDS = 20.0;

void UGameStartSubsystem::OnWorldBeginPlay(UWorld& world)
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
	is_active_ = network->RegisterPacketHandler(S2C_SPAWN, FNetworkPacketHandler::CreateUObject(this, &UGameStartSubsystem::HandleSpawnPacket));
	if (!is_active_)
	{
		return;
	}
	network->RegisterPacketHandler(S2C_PLAY_START, FNetworkPacketHandler::CreateUObject(this, &UGameStartSubsystem::HandlePlayStartPacket));
	network->RegisterPacketHandler(S2C_GAME_READY, FNetworkPacketHandler::CreateUObject(this, &UGameStartSubsystem::HandleGameReadyPacket));
	network->RegisterPacketHandler(S2C_GAME_START, FNetworkPacketHandler::CreateUObject(this, &UGameStartSubsystem::HandleGameStartPacket));
	prepare_start_time_ = world.GetRealTimeSeconds();
	const AGameModeBase* game_mode = world.GetAuthGameMode();
	const APlayerCharacter* character = game_mode && game_mode->DefaultPawnClass ? Cast<APlayerCharacter>(game_mode->DefaultPawnClass->GetDefaultObject()) : nullptr;
	const TArray<FSoftObjectPath> assets = character ? character->GetPreloadAssets() : TArray<FSoftObjectPath>();
	if (!assets.IsEmpty())
	{
		preload_handle_ = UAssetManager::GetStreamableManager().RequestAsyncLoad(assets);
	}
}

void UGameStartSubsystem::Deinitialize()
{
	if (is_active_)
	{
		// 게임을 종료할 때는 게임 인스턴스의 서브시스템이 월드보다 먼저 정리되어 없을 수 있다.
		UGameInstance* game_instance = GetWorld()->GetGameInstance();
		if (UGameNetworkSubsystem* network = game_instance ? game_instance->GetSubsystem<UGameNetworkSubsystem>() : nullptr)
		{
			network->UnregisterPacketHandler(S2C_SPAWN);
			network->UnregisterPacketHandler(S2C_PLAY_START);
			network->UnregisterPacketHandler(S2C_GAME_READY);
			network->UnregisterPacketHandler(S2C_GAME_START);
		}
		is_active_ = false;
	}
	if (timer_widget_)
	{
		timer_widget_->RemoveFromParent();
		timer_widget_ = nullptr;
	}
	preload_handle_.Reset();
	Super::Deinitialize();
}

bool UGameStartSubsystem::DoesSupportWorldType(const EWorldType::Type world_type) const
{
	return world_type == EWorldType::Game || world_type == EWorldType::PIE;
}

bool UGameStartSubsystem::IsTickable() const
{
	return is_active_ && !has_sent_load_complete_;
}

TStatId UGameStartSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGameStartSubsystem, STATGROUP_Tickables);
}

void UGameStartSubsystem::Tick(float delta_seconds)
{
	Super::Tick(delta_seconds);
	// 게임 중에 불러오는 일이 없도록 남은 비동기 로딩, 미리 불러올 에셋, 텍스처, 셰이더 준비가 끝났는지 본다.
	const bool is_ready[] = {
		!IsAsyncLoading(),
		!preload_handle_.IsValid() || preload_handle_->HasLoadCompleted(),
		IStreamingManager::Get().GetNumWantingResources() == 0,
		FShaderPipelineCache::NumPrecompilesRemaining() == 0,
	};
	int32 ready_count = 0;
	for (const bool ready : is_ready)
	{
		ready_count += ready ? 1 : 0;
	}
	ULoadingScreenSubsystem* loading = GetWorld()->GetGameInstance()->GetSubsystem<ULoadingScreenSubsystem>();
	loading->SetProgress(MAP_LOADED_PROGRESS + (1.0f - MAP_LOADED_PROGRESS) * ready_count / UE_ARRAY_COUNT(is_ready));
	if (ready_count == UE_ARRAY_COUNT(is_ready))
	{
		SendLoadComplete();
	}
	else if (GetWorld()->GetRealTimeSeconds() - prepare_start_time_ >= PREPARE_TIMEOUT_SECONDS)
	{
		UE_LOG(LogGameStart, Warning, TEXT("Preparation timed out. async=%d assets=%d textures=%d shaders=%d"), is_ready[0], is_ready[1], is_ready[2], is_ready[3]);
		SendLoadComplete();
	}
}

void UGameStartSubsystem::SendLoadComplete()
{
	has_sent_load_complete_ = true;
	ULoadingScreenSubsystem* loading = GetWorld()->GetGameInstance()->GetSubsystem<ULoadingScreenSubsystem>();
	loading->SetProgress(1.0f);
	loading->SetStatus(LOCTEXT("WaitingPlayers", "다른 플레이어를 기다리는 중..."));
	GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>()->SendPacket(C2S_LOAD_COMPLETE, TArray<uint8>());
}

// 로딩을 마친 플레이어들의 시작 좌표로 캐릭터를 만든다. 늦게 합류한 플레이어의 스폰도 플레이 중에 같은 패킷으로 온다.
void UGameStartSubsystem::HandleSpawnPacket(const TArray<uint8>& payload)
{
	constexpr int32 ENTRIES_OFFSET = static_cast<int32>(offsetof(S2C_SPAWN_PACKET, entries) - sizeof(PACKET_HEADER));
	if (payload.Num() < ENTRIES_OFFSET)
	{
		HandleInvalidPacket();
		return;
	}
	const int32 player_count = payload[0];
	if (player_count > MAX_ROOM_PLAYER_COUNT || payload.Num() != ENTRIES_OFFSET + player_count * static_cast<int32>(sizeof(S2C_SPAWN_ENTRY)))
	{
		HandleInvalidPacket();
		return;
	}
	UWorld* world = GetWorld();
	AThatsMineGameMode* game_mode = world->GetAuthGameMode<AThatsMineGameMode>();
	UMovementSyncSubsystem* movement = world->GetSubsystem<UMovementSyncSubsystem>();
	APlayerController* controller = world->GetFirstPlayerController();
	if (!game_mode || !movement || !controller)
	{
		return;
	}
	const int64 local_player_key = world->GetGameInstance()->GetSubsystem<UOnlineSessionSubsystem>()->GetPlayerKey();
	for (int32 index = 0; index < player_count; ++index)
	{
		S2C_SPAWN_ENTRY entry;
		FMemory::Memcpy(&entry, payload.GetData() + ENTRIES_OFFSET + index * sizeof(S2C_SPAWN_ENTRY), sizeof(entry));
		const FTransform transform = game_mode->FindStartTransform(FTransform(FRotator(0.0, entry.yaw, 0.0), FVector(entry.location[0], entry.location[1], entry.location[2])));
		if (entry.player_id != local_player_key)
		{
			movement->SpawnRemotePlayer(entry.player_id, transform);
		}
		else if (!controller->GetPawn())
		{
			game_mode->RestartPlayerAtTransform(controller, transform);
		}
	}
}

void UGameStartSubsystem::HandlePlayStartPacket(const TArray<uint8>& payload)
{
	if (!payload.IsEmpty())
	{
		HandleInvalidPacket();
		return;
	}
	UClientStateSubsystem* flow = GetWorld()->GetGameInstance()->GetSubsystem<UClientStateSubsystem>();
	if (flow->GetState() == EClientState::LOADING)
	{
		flow->ChangeState(EClientState::PLAYING);
	}
	// 게임 시작 전이면 다른 플레이어를 기다린다. 이미 시작 대기 중이거나 시작했으면 바로 뒤에 오는 패킷이 표시를 바꾼다.
	if (UMatchTimerWidget* timer = ShowTimerWidget())
	{
		timer->SetMessage(LOCTEXT("WaitingOtherPlayers", "다른 플레이어를 기다리는 중입니다..."));
	}
}

// 서버가 시작 대기에 들어갔다. 받은 남은 시간부터 직접 세어 보여 주고, 실제 시작은 서버의 게임 시작 패킷으로 한다.
void UGameStartSubsystem::HandleGameReadyPacket(const TArray<uint8>& payload)
{
	if (payload.Num() != sizeof(S2C_GAME_READY_PACKET) - sizeof(PACKET_HEADER))
	{
		HandleInvalidPacket();
		return;
	}
	std::uint32_t remaining_ms = 0;
	FMemory::Memcpy(&remaining_ms, payload.GetData(), sizeof(remaining_ms));
	has_received_game_ready_ = true;
	if (UMatchTimerWidget* timer = ShowTimerWidget())
	{
		timer->SetMessage(LOCTEXT("GameStartingSoon", "곧 게임을 시작합니다!"));
		timer->StartCountdown(remaining_ms / 1000.0f);
	}
}

void UGameStartSubsystem::HandleGameStartPacket(const TArray<uint8>& payload)
{
	if (!payload.IsEmpty())
	{
		HandleInvalidPacket();
		return;
	}
	if (timer_widget_)
	{
		timer_widget_->RemoveFromParent();
		timer_widget_ = nullptr;
	}
	// 시작 돔을 위에서부터 벗겨 낸다. 게임이 시작된 뒤 합류했으면 연출 없이 바로 숨긴다.
	for (TActorIterator<AStartDome> dome(GetWorld()); dome; ++dome)
	{
		if (has_received_game_ready_)
		{
			dome->StartDissolve();
		}
		else
		{
			dome->HideDome();
		}
	}
}

UMatchTimerWidget* UGameStartSubsystem::ShowTimerWidget()
{
	APlayerController* controller = GetWorld()->GetFirstPlayerController();
	if (!timer_widget_ && controller && controller->IsLocalController())
	{
		timer_widget_ = CreateWidget<UMatchTimerWidget>(controller);
		if (timer_widget_)
		{
			timer_widget_->AddToViewport(10);
		}
	}
	return timer_widget_;
}

void UGameStartSubsystem::HandleInvalidPacket()
{
	UE_LOG(LogGameStart, Warning, TEXT("Invalid game start packet."));
	GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>()->Disconnect();
}

#undef LOCTEXT_NAMESPACE
