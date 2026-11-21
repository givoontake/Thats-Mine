#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "network_connection_state.h"
#include "NetworkConnection.h"
#include "GameNetworkSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNetworkStateChanged, ENetworkConnectionState, state, const FString&, message);
DECLARE_DELEGATE_OneParam(FNetworkPacketHandler, const TArray<uint8>&);

UCLASS(Config = Game, DefaultConfig)
class THATSMINE_API UGameNetworkSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UGameNetworkSubsystem();
	virtual ~UGameNetworkSubsystem() override;
	virtual void Initialize(FSubsystemCollectionBase& collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category = "Network")
	bool Connect(const FString& server_address, int32 server_port = 12312, float connect_timeout_seconds = 3.0f);

	UFUNCTION(BlueprintCallable, Category = "Network")
	void Disconnect();
	bool Reconnect();

	// 쌓아 둔 패킷 뒤에 이 패킷을 붙여 한 번에 보낸다. 서버는 보낸 순서대로 받는다.
	UFUNCTION(BlueprintCallable, Category = "Network")
	bool SendPacket(uint8 type, const TArray<uint8>& payload);
	// 패킷 하나에 헤더를 붙여 buffer 뒤에 이어 쓴다.
	static bool AppendPacket(TArray<uint8>& buffer, uint8 type, const TArray<uint8>& payload);
	// 보내지 않고 쌓아 둔다. 다음에 보내는 패킷 앞에 붙어 순서대로 나간다.
	bool AppendPendingPacket(uint8 type, const TArray<uint8>& payload);
	// 쌓아 둔 패킷을 바로 보낸다.
	bool SendPendingPackets();
	// 쌓아 둔 패킷을 보내지 않고 버린다.
	void ClearPendingPackets() { pending_packets_.Reset(); }

	UFUNCTION(BlueprintPure, Category = "Network")
	ENetworkConnectionState GetConnectionState() const { return connection_state_; }

	UFUNCTION(BlueprintPure, Category = "Network")
	FString GetConnectionMessage() const { return connection_message_; }

	bool RegisterPacketHandler(uint8 type, FNetworkPacketHandler handler);
	void UnregisterPacketHandler(uint8 type);
	void ProcessPackets();

	UPROPERTY(BlueprintAssignable, Category = "Network")
	FOnNetworkStateChanged on_connection_state_changed_;

private:
	bool SendPackets(TArray<uint8>&& packets);

	UPROPERTY(Config)
	FString server_address_ = TEXT("127.0.0.1");

	UPROPERTY(Config)
	int32 server_port_ = 12312;

	UPROPERTY(Config)
	float connect_timeout_seconds_ = 3.0f;

	UPROPERTY(Config)
	bool should_connect_on_initialize_ = true;

	UPROPERTY(Config)
	int32 max_packets_per_frame_ = 128;

	UPROPERTY(Config)
	float packet_budget_ms_ = 2.0f;

	TUniquePtr<NetworkConnection> worker_;
	TMap<uint8, FNetworkPacketHandler> packet_handlers_;
	// 아직 보내지 않은 패킷. 헤더를 붙여 이어 써 두었다가 다음 송신 때 한 번에 보낸다.
	TArray<uint8> pending_packets_;
	ENetworkConnectionState connection_state_ = ENetworkConnectionState::DISCONNECTED;
	FString connection_message_;
	uint64 connection_generation_ = 0;
	bool can_connect_ = false;
	bool is_processing_packets_ = false;
};
