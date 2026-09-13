#include <cstring>
#include "SendBuffer.h"

SendBuffer::SendBuffer()
{
	ex_over.op_type = OPType::SEND;
	wsabuf.len = 0;
	wsabuf.buf = packet_buffer;
}

bool SendBuffer::TrySetInUse()
{
	SendBufferState expected = SendBufferState::AVAILABLE;
	if (!buffer_state.compare_exchange_strong(expected, SendBufferState::IN_USE)) return false;
	Reset();
	return true;
}

bool SendBuffer::Append(const char* data, int data_size)
{
	if (!data || data_size <= 0) return data_size == 0;
	if (buffer_offset + data_size > BUF_SIZE) return false;
	memcpy(packet_buffer + buffer_offset, data, data_size);
	buffer_offset += data_size;
	wsabuf.len = static_cast<ULONG>(buffer_offset);
	return true;
}

void SendBuffer::Clear()
{
	buffer_state.store(SendBufferState::AVAILABLE);
}

void SendBuffer::Reset()
{
	ZeroMemory(&ex_over.over, sizeof(ex_over.over));
	ex_over.session_key = {};
	buffer_offset = 0;
	wsabuf.len = 0;
	wsabuf.buf = packet_buffer;
}

SendBufferPool::SendBufferPool()
{
	for (auto& send_buffer : send_buffers)
		send_buffer.ex_over.op_type = OPType::POOLED_SEND;
}

int SendBufferPool::FindAvailableBuffer()
{
	for (int i = 0; i < send_buffers.size(); ++i) {
		if (send_buffers[i].TrySetInUse()) return i;
	}
	return -1;
}
