#include "LocalSessionSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UI/Local/LocalPlayWidget.h"
#include "UObject/UObjectGlobals.h"
#include "Flow/ClientStateSubsystem.h"

#define LOCTEXT_NAMESPACE "LocalSession"

void ULocalSessionSubsystem::ResetSession()
{
	rooms_.Reset();
	next_room_key_ = 1;
	current_room_key_ = 0;
	FLocalRoom public_room;
	public_room.info.room_key = next_room_key_++;
	public_room.info.room_name = TEXT("로컬 공개 테스트 방");
	public_room.is_sample = true;
	rooms_.Add(MoveTemp(public_room));
	FLocalRoom private_room;
	private_room.info.room_key = next_room_key_++;
	private_room.info.room_name = TEXT("로컬 비공개 테스트 방");
	private_room.info.is_private = true;
	private_room.password = TEXT("1234");
	private_room.is_sample = true;
	rooms_.Add(MoveTemp(private_room));
}

TArray<FLobbyRoomInfo> ULocalSessionSubsystem::GetRooms() const
{
	TArray<FLobbyRoomInfo> rooms;
	rooms.Reserve(rooms_.Num());
	for (const FLocalRoom& room : rooms_)
	{
		rooms.Add(room.info);
	}
	return rooms;
}

TOptional<FLobbyRoomInfo> ULocalSessionSubsystem::GetCurrentRoom() const
{
	for (const FLocalRoom& room : rooms_)
	{
		if (room.info.room_key == current_room_key_)
		{
			return room.info;
		}
	}
	return {};
}

FLocalRoomResult ULocalSessionSubsystem::CreateRoom(const FString& room_name, bool is_private, const FString& password)
{
	if (!GetGameInstance()->GetSubsystem<UClientStateSubsystem>()->IsLocalMode() || current_room_key_ != 0)
	{
		return {false, LOCTEXT("InvalidCreateSession", "로컬 로비에서 방을 만들어 주세요.")};
	}
	const FString name = room_name.TrimStartAndEnd();
	if (name.IsEmpty() || FTCHARToUTF8(*name).Length() > 47)
	{
		return {false, LOCTEXT("InvalidName", "방 이름을 입력해 주세요. 최대 한글 15자 또는 영문 47자입니다.")};
	}
	if (is_private && (password.IsEmpty() || FTCHARToUTF8(*password).Length() > 47))
	{
		return {false, LOCTEXT("InvalidPassword", "비밀번호를 입력해 주세요. 최대 한글 15자 또는 영문 47자입니다.")};
	}
	FLocalRoom room;
	room.info.room_key = next_room_key_++;
	room.info.room_name = name;
	room.info.is_private = is_private;
	room.password = is_private ? password : FString();
	const int64 room_key = room.info.room_key;
	rooms_.Add(MoveTemp(room));
	return JoinRoom(room_key, password);
}

FLocalRoomResult ULocalSessionSubsystem::JoinRoom(int64 room_key, const FString& password)
{
	if (!GetGameInstance()->GetSubsystem<UClientStateSubsystem>()->IsLocalMode() || current_room_key_ != 0)
	{
		return {false, LOCTEXT("InvalidJoinSession", "로컬 로비에서 방을 선택해 주세요.")};
	}
	FLocalRoom* room = rooms_.FindByPredicate([room_key](const FLocalRoom& entry) { return entry.info.room_key == room_key; });
	if (!room)
	{
		return {false, LOCTEXT("MissingRoom", "방을 찾을 수 없습니다.")};
	}
	if (room->info.is_playing || room->info.player_count >= room->info.max_player_count)
	{
		return {false, LOCTEXT("RoomUnavailable", "입장할 수 없는 방입니다.")};
	}
	if (room->info.is_private && room->password != password)
	{
		return {false, LOCTEXT("WrongPassword", "비밀번호가 일치하지 않습니다.")};
	}
	++room->info.player_count;
	current_room_key_ = room_key;
	return {true, FText::GetEmpty(), room->info};
}

void ULocalSessionSubsystem::LeaveRoom()
{
	const int32 index = rooms_.IndexOfByPredicate([this](const FLocalRoom& room) { return room.info.room_key == current_room_key_; });
	if (index != INDEX_NONE)
	{
		FLocalRoom& room = rooms_[index];
		room.info.player_count = FMath::Max(0, room.info.player_count - 1);
		room.info.is_playing = false;
		if (!room.is_sample && room.info.player_count == 0)
		{
			rooms_.RemoveAt(index);
		}
	}
	current_room_key_ = 0;
}


void ULocalSessionSubsystem::Initialize(FSubsystemCollectionBase& collection)
{
	Super::Initialize(collection);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ULocalSessionSubsystem::HandleMapLoaded);
	FWorldDelegates::OnWorldCleanup.AddUObject(this, &ULocalSessionSubsystem::HandleWorldCleanup);
}

void ULocalSessionSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
	FWorldDelegates::OnWorldCleanup.RemoveAll(this);
	ClearGameplayControls();
	rooms_.Reset();
	Super::Deinitialize();
}

void ULocalSessionSubsystem::StartGame()
{
	UClientStateSubsystem* state = GetGameInstance()->GetSubsystem<UClientStateSubsystem>();
	if (!state->IsLocalMode())
	{
		return;
	}
	state->ChangeState(EClientState::PLAYING);
	UGameplayStatics::OpenLevel(this, TEXT("/Game/Thats_Mine/Levels/Demo_Landmass"));
}

void ULocalSessionSubsystem::QuitGame()
{
	UClientStateSubsystem* state = GetGameInstance()->GetSubsystem<UClientStateSubsystem>();
	if (!state->IsLocalMode() || state->GetState() != EClientState::PLAYING)
	{
		return;
	}
	APlayerController* controller = gameplay_controller_.Get();
	ClearGameplayControls();
	UKismetSystemLibrary::QuitGame(this, controller, EQuitPreference::Quit, false);
}

void ULocalSessionSubsystem::HandleMapLoaded(UWorld* world)
{
	if (!world || world->GetGameInstance() != GetGameInstance())
	{
		return;
	}
	UClientStateSubsystem* state = GetGameInstance()->GetSubsystem<UClientStateSubsystem>();
	if (!state->IsLocalMode() || state->GetState() != EClientState::PLAYING)
	{
		return;
	}
	ClearGameplayControls();
	APlayerController* controller = world->GetFirstPlayerController();
	if (!controller || !controller->IsLocalController())
	{
		return;
	}
	gameplay_controller_ = controller;
	gameplay_world_ = world;
	return_input_ = NewObject<UInputComponent>(controller);
	return_input_->Priority = 100;
	return_input_->RegisterComponent();
	return_input_->BindKey(EKeys::F10, IE_Pressed, this, &ULocalSessionSubsystem::QuitGame).bExecuteWhenPaused = true;
	controller->PushInputComponent(return_input_);
	local_play_widget_ = CreateWidget<ULocalPlayWidget>(controller);
	if (local_play_widget_)
	{
		local_play_widget_->AddToViewport(100);
	}
}

void ULocalSessionSubsystem::HandleWorldCleanup(UWorld* world, bool has_session_ended, bool should_cleanup_resources)
{
	if (world == gameplay_world_.Get())
	{
		ClearGameplayControls();
	}
}

void ULocalSessionSubsystem::ClearGameplayControls()
{
	if (local_play_widget_)
	{
		local_play_widget_->RemoveFromParent();
		local_play_widget_ = nullptr;
	}
	if (return_input_)
	{
		if (APlayerController* controller = gameplay_controller_.Get())
		{
			controller->PopInputComponent(return_input_);
		}
		return_input_->DestroyComponent();
		return_input_ = nullptr;
	}
	gameplay_controller_.Reset();
	gameplay_world_.Reset();
}

#undef LOCTEXT_NAMESPACE
