#pragma once
#include <vector>

class Session;

// 방 자리 하나가 한 틱 동안 받는 패킷을 생긴 순서대로 이어 두었다가 송신 한 번으로 보낸다. 그 방을 처리하는 게임 스레드만 쓴다.
class TickSendBuffer
{
	std::vector<char> data_;

public:
	void Append(const void* packet, int packet_size);
	// 모은 패킷을 보내고 비운다. BUF_SIZE 이하면 송신 버퍼 풀로, 넘으면 크기에 맞춘 버퍼로 보낸다.
	void Send(Session& session);
	// 상자 배치처럼 BUF_SIZE를 넘는 묶음은 드물어서, 그만큼 늘어난 메모리는 비울 때 놓는다.
	void Clear();
};
