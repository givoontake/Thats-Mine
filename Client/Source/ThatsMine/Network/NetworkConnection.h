#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "HAL/CriticalSection.h"
#include "network_packet.h"
#include <atomic>

class FEvent;
class FSocket;
class FRunnableThread;
class ISocketSubsystem;
class NetworkSendWorker;
class NetworkRecvWorker;

// 연결된 소켓과 송수신 전용 스레드를 소유한다. 큐의 반대편은 게임 스레드만 사용한다.
class NetworkConnection final
{
public:
	NetworkConnection(FSocket* socket, ISocketSubsystem* socket_subsystem);
	~NetworkConnection();
	bool Start();
	void Stop(const FString& message = FString());
	bool SendPacket(uint8 type, const TArray<uint8>& payload);
	TOptional<NetworkPacket> DequeuePacket();
	bool HasFinished() const { return has_send_finished_.load() && has_recv_finished_.load(); }
	bool HasPendingPackets() const { return recv_count_.load() != 0; }
	FString GetCompletionMessage() const;

private:
	friend class NetworkSendWorker;
	friend class NetworkRecvWorker;

	FSocket* socket_ = nullptr;
	ISocketSubsystem* socket_subsystem_ = nullptr;
	FEvent* send_event_ = nullptr;
	TUniquePtr<NetworkSendWorker> send_worker_;
	TUniquePtr<NetworkRecvWorker> recv_worker_;
	TUniquePtr<FRunnableThread> send_thread_;
	TUniquePtr<FRunnableThread> recv_thread_;
	std::atomic<bool> is_stopping_{ false };
	std::atomic<bool> has_send_finished_{ false };
	std::atomic<bool> has_recv_finished_{ false };
	std::atomic<int32> send_count_{ 0 };
	std::atomic<int32> recv_count_{ 0 };
	TQueue<TArray<uint8>, EQueueMode::Spsc> send_queue_;
	TQueue<NetworkPacket, EQueueMode::Spsc> recv_queue_;
	FCriticalSection stop_mutex_;
	// FSocket의 송수신은 내부 LastActivityTime도 갱신한다. 짧은 비차단 호출만 보호하며 대기는 잠그지 않는다.
	FCriticalSection socket_mutex_;
	FString completion_message_;
};
