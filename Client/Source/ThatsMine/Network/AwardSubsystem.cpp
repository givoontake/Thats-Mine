#include "AwardSubsystem.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Environment/AwardStage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Flow/ClientStateSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Network/GameNetworkSubsystem.h"
#include "Network/OnlineSessionSubsystem.h"
#include "Network/game_protocol.h"
#include "Player/PlayerCharacter.h"
#include "TimerManager.h"
#include "UI/Award/AwardWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogAward, Log, All);

static constexpr TCHAR AWARD_SOUND_PATH[] = TEXT("/Game/Thats_Mine/Effects/Sounds/SW_Award.SW_Award");

void UAwardSubsystem::OnWorldBeginPlay(UWorld& world)
{
	Super::OnWorldBeginPlay(world);
	UGameInstance* game_instance = world.GetGameInstance();
	const UClientStateSubsystem* flow = game_instance ? game_instance->GetSubsystem<UClientStateSubsystem>() : nullptr;
	// 온라인 게임 레벨은 로딩 상태에서 열린다.
	if (!flow || flow->IsLocalMode() || flow->GetState() != EClientState::LOADING)
	{
		return;
	}
	is_active_ = game_instance->GetSubsystem<UGameNetworkSubsystem>()->RegisterPacketHandler(S2C_AWARD, FNetworkPacketHandler::CreateUObject(this, &UAwardSubsystem::HandleAwardPacket));
}

void UAwardSubsystem::Deinitialize()
{
	if (is_active_)
	{
		// 게임이 종료될 때는 게임 인스턴스 서브시스템이 월드보다 먼저 정리되어 없을 수 있다.
		UGameInstance* game_instance = GetWorld()->GetGameInstance();
		if (UGameNetworkSubsystem* network = game_instance ? game_instance->GetSubsystem<UGameNetworkSubsystem>() : nullptr)
		{
			network->UnregisterPacketHandler(S2C_AWARD);
		}
		is_active_ = false;
	}
	Super::Deinitialize();
}

bool UAwardSubsystem::DoesSupportWorldType(const EWorldType::Type world_type) const
{
	return world_type == EWorldType::Game || world_type == EWorldType::PIE;
}

// 부문 하나의 순위. 같은 틱에 부문 순서대로 모두 오므로, 첫 부문을 받으면 다음 틱에 시상식을 시작한다.
void UAwardSubsystem::HandleAwardPacket(const TArray<uint8>& payload)
{
	S2C_AWARD_PACKET packet;
	if (payload.Num() != sizeof(packet) - sizeof(PACKET_HEADER))
	{
		UE_LOG(LogAward, Warning, TEXT("Invalid award packet."));
		GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>()->Disconnect();
		return;
	}
	FMemory::Memcpy(reinterpret_cast<uint8*>(&packet) + sizeof(PACKET_HEADER), payload.GetData(), payload.Num());
	if (packet.category >= static_cast<uint8>(AwardCategory::COUNT) || scene_index_ != INDEX_NONE)
	{
		return;
	}
	// 순위는 부문마다 1위부터 이어 온다. 부문이 바뀌면 새 장면을 만들고, 같은 부문이면 그 장면에 다음 순위를 더한다.
	const bool is_new_scene = scenes_.IsEmpty() || scenes_.Last().category != packet.category;
	if (!is_new_scene && scenes_.Last().nicknames.Num() >= AWARD_RANK_COUNT)
	{
		return;
	}
	const bool is_box_open = packet.category == static_cast<uint8>(AwardCategory::BOX_OPEN);
	if (is_new_scene)
	{
		FAwardScene& scene = scenes_.AddDefaulted_GetRef();
		scene.category = packet.category;
		scene.title = is_box_open ? NSLOCTEXT("Award", "BoxOpen", "탐색왕") : NSLOCTEXT("Award", "Stun", "기절왕");
	}
	FAwardScene& scene = scenes_.Last();
	const TArray<FRoomMemberInfo>& members = GetWorld()->GetGameInstance()->GetSubsystem<UOnlineSessionSubsystem>()->GetMembers();
	const FRoomMemberInfo* member = members.FindByPredicate([&packet](const FRoomMemberInfo& info) { return info.player_key == packet.player_id; });
	scene.nicknames.Add(member ? member->nickname : FString());
	scene.records.Add(FText::Format(is_box_open ? NSLOCTEXT("Award", "BoxOpenRecord", "상자 {0}개") : NSLOCTEXT("Award", "StunRecord", "기절 {0}회"), FText::AsNumber(packet.count)));
	if (is_new_scene && scenes_.Num() == 1)
	{
		GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UAwardSubsystem::StartCeremony);
	}
}

// 게임 화면 위젯을 모두 내리고, 플레이어 캐릭터를 숨기고 시상 무대를 보이고, 입력을 시상식 화면으로만 받으며 시상 무대 카메라로 바꾼다.
// 시상식 화면만 입력을 받으면 누르고 있던 이동 키가 떼어지지 않은 채 남으므로, 눌린 키를 비우고 캐릭터 입력을 끈다.
// 캐릭터는 멈추고 애니메이션을 세워 발소리 같은 게임 소리가 나지 않게 한다.
void UAwardSubsystem::StartCeremony()
{
	APlayerController* controller = GetWorld()->GetFirstPlayerController();
	TActorIterator<AAwardStage> stage(GetWorld());
	if (!controller || !stage)
	{
		UE_LOG(LogAward, Warning, TEXT("Award stage or player controller is missing."));
		FinishCeremony();
		return;
	}
	stage_ = *stage;
	stage_->ShowStage();
	award_sound_ = LoadObject<USoundBase>(nullptr, AWARD_SOUND_PATH);
	UWidgetLayoutLibrary::RemoveAllWidgets(this);
	controller->FlushPressedKeys();
	for (TActorIterator<APlayerCharacter> character(GetWorld()); character; ++character)
	{
		character->SetActorHiddenInGame(true);
		character->DisableInput(controller);
		character->GetCharacterMovement()->StopMovementImmediately();
		character->GetMesh()->bPauseAnims = true;
	}
	widget_ = CreateWidget<UAwardWidget>(controller, UAwardWidget::StaticClass());
	widget_->AddToViewport(100);
	FInputModeUIOnly input_mode;
	input_mode.SetWidgetToFocus(widget_->TakeWidget());
	controller->SetInputMode(input_mode);
	controller->SetShowMouseCursor(true);
	controller->SetViewTarget(stage_);
	scene_index_ = 0;
	ShowScene();
}

void UAwardSubsystem::ShowScene()
{
	is_switching_ = false;
	APlayerController* controller = GetWorld()->GetFirstPlayerController();
	const APlayerCharacter* character = controller ? Cast<APlayerCharacter>(controller->GetPawn()) : nullptr;
	const FAwardScene& scene = scenes_[scene_index_];
	stage_->ShowFigures(scene.nicknames.Num(), character ? character->GetMesh() : nullptr);
	widget_->ShowScene(stage_, scene.title, scene.nicknames, scene.records);
	if (controller && controller->PlayerCameraManager)
	{
		controller->PlayerCameraManager->StartCameraFade(1.0f, 0.0f, FADE_SECONDS, FLinearColor::Black);
	}
	if (award_sound_)
	{
		UGameplayStatics::PlaySound2D(this, award_sound_);
	}
	GetWorld()->GetTimerManager().SetTimer(scene_timer_, this, &UAwardSubsystem::Skip, AWARD_SCENE_MS / 1000.0f, false);
}

void UAwardSubsystem::Skip()
{
	if (scene_index_ == INDEX_NONE || is_switching_ || is_finished_)
	{
		return;
	}
	GetWorld()->GetTimerManager().ClearTimer(scene_timer_);
	if (scene_index_ + 1 >= scenes_.Num())
	{
		FinishCeremony();
		return;
	}
	// 어두워진 뒤 다음 부문으로 바꾼다.
	is_switching_ = true;
	++scene_index_;
	APlayerController* controller = GetWorld()->GetFirstPlayerController();
	if (controller && controller->PlayerCameraManager)
	{
		controller->PlayerCameraManager->StartCameraFade(0.0f, 1.0f, FADE_SECONDS, FLinearColor::Black, false, true);
	}
	GetWorld()->GetTimerManager().SetTimer(scene_timer_, this, &UAwardSubsystem::ShowScene, FADE_SECONDS, false);
}

// 시상식을 다 봤다고 서버에 알린다. 서버가 방 복귀를 보낼 때까지 마지막 화면을 그대로 둔다.
void UAwardSubsystem::FinishCeremony()
{
	if (is_finished_)
	{
		return;
	}
	is_finished_ = true;
	if (is_active_)
	{
		GetWorld()->GetGameInstance()->GetSubsystem<UGameNetworkSubsystem>()->SendPacket(C2S_AWARD_DONE, TArray<uint8>());
	}
}
