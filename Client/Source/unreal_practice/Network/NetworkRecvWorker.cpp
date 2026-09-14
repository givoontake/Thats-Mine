#include "NetworkRecvWorker.h"

#include "NetworkConnection.h"
#include "Misc/ScopeLock.h"
#include "Sockets.h"

NetworkRecvWorker::NetworkRecvWorker(NetworkConnection& connection)
	: connection_(connection)
{
	recv_buffer_.Reserve(NetworkLimits::MAX_PACKET_SIZE * 2);
}

uint32 NetworkRecvWorker::Run()
{
	while (!connection_.is_stopping_.load())
	{
		if (connection_.socket_->Wait(ESocketWaitConditions::WaitForRead, FTimespan::FromMilliseconds(100)))
		{
			if (connection_.is_stopping_.load() || !RecvPackets())
			{
				break;
			}
		}
		else
		{
			bool has_socket_error = false;
			{
				FScopeLock lock(&connection_.socket_mutex_);
				has_socket_error = connection_.socket_->GetConnectionState() == SCS_ConnectionError;
			}
			if (has_socket_error)
			{
				connection_.Stop(TEXT("Recv wait failed or connection lost."));
				break;
			}
		}
	}
	// 메시지와 마지막 수신 패킷을 게시한 뒤 완료 상태를 알린다.
	connection_.has_recv_finished_.store(true);
	return 0;
}

void NetworkRecvWorker::Stop()
{
	connection_.Stop();
}

bool NetworkRecvWorker::RecvPackets()
{
	uint8 recv_data[NetworkLimits::MAX_PACKET_SIZE];
	int32 recv_bytes = 0;
	bool is_recv_success = false;
	{
		FScopeLock lock(&connection_.socket_mutex_);
		is_recv_success = connection_.socket_->Recv(recv_data, sizeof(recv_data), recv_bytes);
	}
	if (!is_recv_success)
	{
		connection_.Stop(recv_buffer_.IsEmpty() ? TEXT("Connection closed or recv failed.") : TEXT("Connection closed with an incomplete packet."));
		return false;
	}
	// 언리얼 스트림 소켓은 would-block도 성공/0바이트로 반환한다.
	if (recv_bytes == 0)
	{
		return true;
	}
	recv_buffer_.Append(recv_data, recv_bytes);
	int32 offset = 0;
	while (recv_buffer_.Num() - offset >= NetworkLimits::HEADER_SIZE)
	{
		const uint8* data = recv_buffer_.GetData() + offset;
		const int32 packet_size = static_cast<int32>(data[0]) | (static_cast<int32>(data[1]) << 8);
		if (packet_size < NetworkLimits::HEADER_SIZE || packet_size > NetworkLimits::MAX_PACKET_SIZE)
		{
			connection_.Stop(TEXT("Invalid packet size."));
			return false;
		}
		if (recv_buffer_.Num() - offset < packet_size)
		{
			break;
		}
		if (connection_.recv_count_.load() >= NetworkLimits::MAX_QUEUED_PACKETS)
		{
			connection_.Stop(TEXT("Recv queue limit exceeded."));
			return false;
		}
		NetworkPacket packet;
		packet.type = data[2];
		packet.payload.Append(data + NetworkLimits::HEADER_SIZE, packet_size - NetworkLimits::HEADER_SIZE);
		connection_.recv_count_.fetch_add(1);
		connection_.recv_queue_.Enqueue(MoveTemp(packet));
		offset += packet_size;
	}
	if (offset != 0)
	{
		recv_buffer_.RemoveAt(0, offset, EAllowShrinking::No);
	}
	return true;
}
