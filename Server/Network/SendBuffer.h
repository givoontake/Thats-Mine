#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include "ExOverlapped.h"

enum class SendBufferState : std::uint8_t
{
	AVAILABLE,
	IN_USE
};

struct SendBuffer : public IOOverlapped
{
	int buffer_offset = 0;
	std::atomic<SendBufferState> buffer_state = SendBufferState::AVAILABLE;

	SendBuffer();
	bool TrySetInUse();
	bool Append(const char* data, int data_size);
	void Clear();

private:
	void Reset();
};

constexpr int SEND_BUFFER_POOL_SIZE = 5;

struct SendBufferPool
{
	std::array<SendBuffer, SEND_BUFFER_POOL_SIZE> send_buffers;

	SendBufferPool();
	int FindAvailableBuffer();
};
