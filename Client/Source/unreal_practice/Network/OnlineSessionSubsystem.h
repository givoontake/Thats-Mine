#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UI/Lobby/lobby_room_info.h"
#include "UI/Room/room_member_info.h"
#include "Flow/client_state.h"
#include "network_connection_state.h"
#include "online_request.h"
#include "OnlineSessionSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE(FOnOnlineSessionChanged);

UCLASS()
class UNREAL_PRACTICE_API UOnlineSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& collection) override;
	virtual void Deinitialize() override;
	void Login(const FString& user_id, const FString& password);
	void RefreshRooms();
	void CreateRoom(const FString& room_name, bool is_private, const FString& password);
	void JoinRoom(int64 room_key, const FString& password);
	void QuickJoin();
	void LeaveRoom();
	void SetReady(bool is_ready);
	void StartGame();
	void KickPlayer(int64 player_key);
	const TArray<FLobbyRoomInfo>& GetRooms() const { return rooms_; }
	const FLobbyRoomInfo& GetRoom() const { return room_; }
	const TArray<FRoomMemberInfo>& GetMembers() const { return members_; }
	int64 GetPlayerKey() const { return player_key_; }
	const FText& GetStatus() const { return status_; }
	bool IsPending() const { return pending_request_ != OnlineRequest::NONE; }
	bool CanStart() const;
	bool IsLoggedIn() const { return player_key_ > 0; }
	FOnOnlineSessionChanged on_changed_;

private:
	void HandlePacket(const TArray<uint8>& payload, uint8 type);
	UFUNCTION()
	void HandleConnectionChanged(ENetworkConnectionState state, const FString& message);
	UFUNCTION()
	void HandleClientStateChanged(EClientState state);
	bool CanRequest(EClientState state) const;
	void BeginRequest(OnlineRequest request);
	void FinishRequest();
	void HandleTimeout();
	void NotifyChanged();
	void FlushChanges();
	void ShowStatus(const FText& message);
	void ResetSession();
	void EnterRoom(const FLobbyRoomInfo& room);
	void ReturnToLogin(const FText& message);
	void HandleMapLoaded(UWorld* world);
	TArray<FLobbyRoomInfo> rooms_;
	FLobbyRoomInfo room_;
	TArray<FRoomMemberInfo> members_;
	int64 player_key_ = 0;
	int64 host_key_ = 0;
	int64 pending_kick_key_ = 0;
	FString nickname_;
	FText status_;
	OnlineRequest pending_request_ = OnlineRequest::NONE;
	FTimerHandle request_timer_;
	FTimerHandle refresh_timer_;
	bool has_pending_refresh_ = false;
	bool is_kicked_ = false;
};
