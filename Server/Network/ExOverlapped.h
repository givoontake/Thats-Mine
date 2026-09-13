#pragma once
#include <cstdint>
#include <memory>
#include <WinSock2.h>
#include <MSWSock.h>
#include "settings.h"
#include "enum_class.h"

struct SessionKey {
	int session_index = -1;
	int player_id = -1;
	std::uint64_t session_id = 0;
};

constexpr ULONG_PTR LISTEN_IO_COMPLETION = 1;
constexpr ULONG_PTR SESSION_IO_COMPLETION = 2;

struct ExOverlapped {
	WSAOVERLAPPED over;
	OPType op_type;
	SessionKey session_key;

	ExOverlapped() {
		ZeroMemory(&over, sizeof(over));
	}
};

struct IOOverlapped {
	ExOverlapped ex_over;
	WSABUF wsabuf;
	char packet_buffer[BUF_SIZE];

	IOOverlapped()
	{
		wsabuf.len = BUF_SIZE;
		wsabuf.buf = packet_buffer;
	}

	void SetOperationType(OPType op_type) {
		ex_over.op_type = op_type;
	}

};
