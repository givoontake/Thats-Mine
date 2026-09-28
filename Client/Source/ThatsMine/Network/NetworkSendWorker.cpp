#include "NetworkSendWorker.h"

#include "NetworkConnection.h"
#include "HAL/Event.h"
#include "Misc/ScopeLock.h"
#include "SocketSubsystem.h"
#include "Sockets.h"

NetworkSendWorker::NetworkSendWorker(NetworkConnection& connection)
	: connection_(connection)
{
}

uint32 NetworkSendWorker::Run()
{
	while (!connection_.is_stopping_.load())
	{
		TArray<uint8> pending_send;
		if (!connection_.send_queue_.Dequeue(pending_send))
		{
			// 큐 게시 후 이벤트를 깨우므로 확인과 대기 사이에 송신이 들어와도 놓치지 않는다.
			connection_.send_event_->Wait();
			continue;
		}
		int32 send_offset = 0;
		while (send_offset < pending_send.Num() && !connection_.is_stopping_.load())
		{
			int32 sent_bytes = 0;
			bool is_sent = false;
			ESocketErrors error = SE_NO_ERROR;
			{
				FScopeLock lock(&connection_.socket_mutex_);
				is_sent = connection_.socket_->Send(pending_send.GetData() + send_offset, pending_send.Num() - send_offset, sent_bytes);
				if (!is_sent)
				{
					error = connection_.socket_subsystem_->GetLastErrorCode();
				}
			}
			if (!is_sent)
			{
				if (error != SE_EWOULDBLOCK && error != SE_EINTR)
				{
					connection_.Stop(FString::Printf(TEXT("Send failed (%d)."), static_cast<int32>(error)));
					break;
				}
				// 전송이 막힌 동안에는 쓰기 가능 상태를 기다린다. 수신 스레드는 독립적으로 진행한다.
				if (!connection_.socket_->Wait(ESocketWaitConditions::WaitForWrite, FTimespan::FromMilliseconds(100)))
				{
					bool has_socket_error = false;
					{
						FScopeLock lock(&connection_.socket_mutex_);
						has_socket_error = connection_.socket_->GetConnectionState() == SCS_ConnectionError;
					}
					if (has_socket_error)
					{
						connection_.Stop(TEXT("Send wait failed or connection lost."));
					}
				}
				continue;
			}
			if (sent_bytes <= 0)
			{
				connection_.Stop(TEXT("Send returned zero bytes."));
				break;
			}
			send_offset += sent_bytes;
		}
		connection_.send_count_.fetch_sub(1);
	}
	connection_.has_send_finished_.store(true);
	return 0;
}

void NetworkSendWorker::Stop()
{
	connection_.Stop();
}
