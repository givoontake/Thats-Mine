#pragma once
#include <cstdint>
#include <memory>
#include <WinSock2.h>
#include <MSWSock.h>
#include "settings.h"
#include "enum_class.h"
#include "types.h"

constexpr ULONG_PTR LISTEN_IO_COMPLETION = 1;
constexpr ULONG_PTR SESSION_IO_COMPLETION = 2;
constexpr ULONG_PTR DB_SESSION_COMPLETION = 3;

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
