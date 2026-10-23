#pragma once
#include <WinSock2.h>
#include "settings.h"

enum class OperationType
{
	SEND,
	RECV,
	CONNECT
};

// 보내기와 연결에 쓰는 겹친 입출력 구조. 보내기는 패킷 하나를 담아 보내고 완료되면 지운다.
struct ExOverlapped
{
	WSAOVERLAPPED over{};
	WSABUF wsabuf{};
	char packet_buffer[BUF_SIZE]{};
	OperationType operation_type = OperationType::SEND;
};
