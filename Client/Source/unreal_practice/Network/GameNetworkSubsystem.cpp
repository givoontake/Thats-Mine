#include "GameNetworkSubsystem.h"

#include "HAL/PlatformTime.h"
#include "Engine/GameInstance.h"
#include "Flow/ClientStateSubsystem.h"
#include "IPAddress.h"
#include "Templates/UnrealTemplate.h"
#include "SocketSubsystem.h"
#include "Sockets.h"

DEFINE_LOG_CATEGORY_STATIC(LogGameNetwork, Log, All);

UGameNetworkSubsystem::UGameNetworkSubsystem() = default;
UGameNetworkSubsystem::~UGameNetworkSubsystem() = default;

void UGameNetworkSubsystem::Initialize(FSubsystemCollectionBase& collection)
{
	Super::Initialize(collection);
	collection.InitializeDependency<UClientStateSubsystem>();
	can_connect_ = true;
	if (should_connect_on_initialize_)
	{
		Connect(server_address_, server_port_, connect_timeout_seconds_);
	}
}

void UGameNetworkSubsystem::Deinitialize()
{
	can_connect_ = false;
	Disconnect();
	packet_handlers_.Empty();
	on_connection_state_changed_.Clear();
	Super::Deinitialize();
}

bool UGameNetworkSubsystem::Connect(const FString& server_address, int32 server_port, float connect_timeout_seconds)
{
	check(IsInGameThread());
	if (!can_connect_ || GetGameInstance()->GetSubsystem<UClientStateSubsystem>()->IsLocalMode())
	{
		return false;
	}
	// 연결별 큐도 함께 파괴해 이전 연결의 패킷이 재접속 후 적용되지 않게 한다.
	++connection_generation_;
	worker_.Reset();
	connection_state_ = ENetworkConnectionState::DISCONNECTED;
	ISocketSubsystem* sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	FSocket* socket = nullptr;
	auto fail_connection = [&](const FString& message)
	{
		if (socket)
		{
			socket->Close();
			sockets->DestroySocket(socket);
		}
		connection_state_ = ENetworkConnectionState::FAILED;
		connection_message_ = message;
		UE_LOG(LogGameNetwork, Warning, TEXT("%s"), *message);
		on_connection_state_changed_.Broadcast(connection_state_, message);
		return false;
	};
	if (!sockets || server_port < 1 || server_port > 65535 || !FMath::IsFinite(connect_timeout_seconds) || connect_timeout_seconds <= 0.0f || connect_timeout_seconds > 30.0f)
	{
		return fail_connection(TEXT("Invalid connection settings or socket subsystem unavailable."));
	}
	TSharedRef<FInternetAddr> address = sockets->CreateInternetAddr(FNetworkProtocolTypes::IPv4);
	bool is_valid_address = false;
	address->SetIp(*server_address, is_valid_address);
	address->SetPort(server_port);
	if (!is_valid_address)
	{
		return fail_connection(TEXT("Server address must be a numeric IPv4 address."));
	}
	socket = sockets->CreateSocket(NAME_Stream, TEXT("Game connection"), FNetworkProtocolTypes::IPv4);
	if (!socket || !socket->SetNonBlocking(true) || !socket->SetNoDelay(true))
	{
		return fail_connection(TEXT("Could not configure the connection socket."));
	}
	// Unreal의 Connect는 연결 진행 중에도 true다. 쓰기 가능 여부와 실제 상대 주소를 확인한다.
	TSharedRef<FInternetAddr> peer_address = sockets->CreateInternetAddr(FNetworkProtocolTypes::IPv4);
	if (!socket->Connect(*address) || !socket->Wait(ESocketWaitConditions::WaitForWrite, FTimespan::FromSeconds(connect_timeout_seconds)) || socket->GetConnectionState() != SCS_Connected || !socket->GetPeerAddress(*peer_address))
	{
		return fail_connection(TEXT("Server connection failed or timed out."));
	}
	worker_ = MakeUnique<NetworkConnection>(socket, sockets);
	socket = nullptr;
	if (!worker_->Start())
	{
		worker_.Reset();
		return fail_connection(TEXT("Could not start the network threads."));
	}
	connection_state_ = ENetworkConnectionState::CONNECTED;
	connection_message_.Reset();
	const FString message;
	on_connection_state_changed_.Broadcast(connection_state_, message);
	return true;
}

void UGameNetworkSubsystem::Disconnect()
{
	check(IsInGameThread());
	++connection_generation_;
	worker_.Reset();
	const bool should_notify = connection_state_ != ENetworkConnectionState::DISCONNECTED;
	connection_state_ = ENetworkConnectionState::DISCONNECTED;
	connection_message_.Reset();
	if (should_notify)
	{
		const FString message;
		on_connection_state_changed_.Broadcast(connection_state_, message);
	}
}

bool UGameNetworkSubsystem::SendPacket(uint8 type, const TArray<uint8>& payload)
{
	check(IsInGameThread());
	return !GetGameInstance()->GetSubsystem<UClientStateSubsystem>()->IsLocalMode() && worker_ && connection_state_ == ENetworkConnectionState::CONNECTED && worker_->SendPacket(type, payload);
}

bool UGameNetworkSubsystem::RegisterPacketHandler(uint8 type, FNetworkPacketHandler handler)
{
	check(IsInGameThread());
	if (!handler.IsBound() || packet_handlers_.Contains(type))
	{
		return false;
	}
	packet_handlers_.Add(type, MoveTemp(handler));
	return true;
}

void UGameNetworkSubsystem::UnregisterPacketHandler(uint8 type)
{
	check(IsInGameThread());
	packet_handlers_.Remove(type);
}

void UGameNetworkSubsystem::ProcessPackets()
{
	check(IsInGameThread());
	if (!worker_ || is_processing_packets_)
	{
		return;
	}
	TGuardValue<bool> processing_guard(is_processing_packets_, true);
	const uint64 generation = connection_generation_;
	const int32 packet_limit = FMath::Clamp(max_packets_per_frame_, 1, NetworkLimits::MAX_QUEUED_PACKETS);
	const double budget_seconds = FMath::IsFinite(packet_budget_ms_) ? FMath::Clamp(packet_budget_ms_, 0.1f, 10.0f) / 1000.0 : 0.002;
	const double deadline = FPlatformTime::Seconds() + budget_seconds;
	for (int32 processed = 0; processed < packet_limit && FPlatformTime::Seconds() < deadline; ++processed)
	{
		TOptional<NetworkPacket> packet = worker_->DequeuePacket();
		if (!packet.IsSet())
		{
			break;
		}
		if (const FNetworkPacketHandler* handler = packet_handlers_.Find(packet->type))
		{
			// 처리 함수 안에서 등록 해제나 재접속을 해도 현재 호출은 유효하게 유지한다.
			FNetworkPacketHandler current_handler = *handler;
			current_handler.ExecuteIfBound(packet->payload);
		}
		if (generation != connection_generation_)
		{
			return;
		}
	}
	// 종료 직전에 수신된 정상 패킷은 먼저 전달한다.
	if (worker_->HasFinished() && !worker_->HasPendingPackets())
	{
		const FString message = worker_->GetCompletionMessage();
		++connection_generation_;
		worker_.Reset();
		connection_state_ = ENetworkConnectionState::DISCONNECTED;
		connection_message_ = message;
		UE_LOG(LogGameNetwork, Log, TEXT("%s"), *message);
		on_connection_state_changed_.Broadcast(connection_state_, message);
	}
}

bool UGameNetworkSubsystem::Reconnect()
{
	return Connect(server_address_, server_port_, connect_timeout_seconds_);
}
