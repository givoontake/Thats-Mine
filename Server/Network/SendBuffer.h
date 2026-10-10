#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <vector>
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

// BUF_SIZE보다 큰 패킷을 보낼 때 패킷 크기에 맞춰 새로 만드는 송신 버퍼. 송신이 끝나면 입출력 스레드가 지운다.
// 완료 통지로 받은 ExOverlapped 주소를 이 구조체로 바꿔 쓰므로 ex_over가 첫 멤버여야 한다.
struct LargeSendBuffer
{
	ExOverlapped ex_over;
	WSABUF wsabuf;
	std::vector<char> packet;

	LargeSendBuffer(const char* data, int data_size);
};

constexpr int SEND_BUFFER_POOL_SIZE = 5;

struct SendBufferPool
{
	std::array<SendBuffer, SEND_BUFFER_POOL_SIZE> send_buffers;

	SendBufferPool();
	int FindAvailableBuffer();
};
