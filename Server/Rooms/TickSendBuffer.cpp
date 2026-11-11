#include "TickSendBuffer.h"
#include "Session.h"
#include "server_settings.h"

void TickSendBuffer::Append(const void* packet, int packet_size)
{
	const char* bytes = static_cast<const char*>(packet);
	data_.insert(data_.end(), bytes, bytes + packet_size);
}

void TickSendBuffer::Send(Session& session)
{
	if (data_.empty()) return;
	const int data_size = static_cast<int>(data_.size());
	if (data_size <= BUF_SIZE) session.SendPacket(data_.data(), data_size);
	else session.SendLargePacket(data_.data(), data_size);
	Clear();
}

void TickSendBuffer::Clear()
{
	if (data_.capacity() > BUF_SIZE) data_ = {};
	else data_.clear();
}
