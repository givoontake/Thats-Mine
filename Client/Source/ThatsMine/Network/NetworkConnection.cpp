#include "NetworkConnection.h"

#include "NetworkRecvWorker.h"
#include "NetworkSendWorker.h"
#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"
#include "HAL/RunnableThread.h"
#include "Misc/ScopeLock.h"
#include "SocketSubsystem.h"
#include "Sockets.h"

NetworkConnection::NetworkConnection(FSocket* socket, ISocketSubsystem* socket_subsystem)
	: socket_(socket), socket_subsystem_(socket_subsystem)
{
	send_event_ = FPlatformProcess::GetSynchEventFromPool(false);
	send_worker_ = MakeUnique<NetworkSendWorker>(*this);
	recv_worker_ = MakeUnique<NetworkRecvWorker>(*this);
}

NetworkConnection::~NetworkConnection()
{
	Stop();
	if (send_thread_)
	{
		send_thread_->WaitForCompletion();
		send_thread_.Reset();
	}
	if (recv_thread_)
	{
		recv_thread_->WaitForCompletion();
		recv_thread_.Reset();
	}
	send_worker_.Reset();
	recv_worker_.Reset();
	socket_->Close();
	socket_subsystem_->DestroySocket(socket_);
	FPlatformProcess::ReturnSynchEventToPool(send_event_);
}

bool NetworkConnection::Start()
{
	check(IsInGameThread());
	check(!send_thread_ && !recv_thread_);
	send_thread_.Reset(FRunnableThread::Create(send_worker_.Get(), TEXT("GameNetworkSend")));
	if (!send_thread_)
	{
		Stop(TEXT("Could not start the send thread."));
		return false;
	}
	recv_thread_.Reset(FRunnableThread::Create(recv_worker_.Get(), TEXT("GameNetworkRecv")));
	if (!recv_thread_)
	{
		Stop(TEXT("Could not start the recv thread."));
		return false;
	}
	return true;
}

void NetworkConnection::Stop(const FString& message)
{
	{
		FScopeLock lock(&stop_mutex_);
		if (is_stopping_.load())
		{
			return;
		}
		// 먼저 발생한 종료 원인만 기록하고 두 스레드에 함께 종료를 알린다.
		completion_message_ = message;
		is_stopping_.store(true);
	}
	send_event_->Trigger();
	FScopeLock lock(&socket_mutex_);
	// 소켓 파괴는 두 스레드 합류 이후에만 수행한다.
	socket_->Shutdown(ESocketShutdownMode::ReadWrite);
}

bool NetworkConnection::SendPacket(uint8 type, const TArray<uint8>& payload)
{
	check(IsInGameThread());
	if (is_stopping_.load() || payload.Num() > NetworkLimits::MAX_PACKET_SIZE - NetworkLimits::HEADER_SIZE || send_count_.load() >= NetworkLimits::MAX_QUEUED_PACKETS)
	{
		return false;
	}
	const int32 packet_size = NetworkLimits::HEADER_SIZE + payload.Num();
	TArray<uint8> packet;
	packet.SetNumUninitialized(packet_size);
	// 서버의 Windows little-endian 헤더. 길이는 헤더를 포함한다.
	packet[0] = static_cast<uint8>(packet_size & 0xff);
	packet[1] = static_cast<uint8>((packet_size >> 8) & 0xff);
	packet[2] = type;
	if (!payload.IsEmpty())
	{
		FMemory::Memcpy(packet.GetData() + NetworkLimits::HEADER_SIZE, payload.GetData(), payload.Num());
	}
	send_count_.fetch_add(1);
	send_queue_.Enqueue(MoveTemp(packet));
	send_event_->Trigger();
	return true;
}

TOptional<NetworkPacket> NetworkConnection::DequeuePacket()
{
	check(IsInGameThread());
	NetworkPacket packet;
	if (!recv_queue_.Dequeue(packet))
	{
		return {};
	}
	recv_count_.fetch_sub(1);
	return TOptional<NetworkPacket>(MoveTemp(packet));
}

FString NetworkConnection::GetCompletionMessage() const
{
	check(HasFinished());
	return completion_message_;
}
