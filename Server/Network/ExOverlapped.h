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
constexpr ULONG_PTR DISCONNECT_CLEANUP_COMPLETION = 3;
constexpr ULONG_PTR ACCEPT_RETRY_COMPLETION = 4;

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

// 접속 수락 요청 하나의 정보. 수락용 소켓을 함께 두어, 완료 통지로 받은 주소를 이 구조체로 바꿔 어느 소켓이 접속을 받았는지 안다.
// 받은 주소가 곧 이 구조체의 주소가 되도록 IOOverlapped를 상속해 맨 앞에 둔다.
struct AcceptOverlapped : public IOOverlapped {
	SOCKET accept_socket = INVALID_SOCKET;

	AcceptOverlapped() {
		SetOperationType(OPType::ACCEPT);
	}
};
