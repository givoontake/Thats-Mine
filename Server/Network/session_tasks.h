#pragma once
#include <vector>
#include "types.h"

// 입출력 스레드가 받은 패킷 하나. 받을 때의 세션 상태로 정한 처리 단위(로그인, 로비, 방)의 큐에 넣는다.
// 처리하는 쪽은 세션 키로 세션을 찾고, 지금도 자기 소속일 때만 처리한다.
struct SessionPacketTask
{
	SessionKey session_key;
	std::vector<char> packet;
	long long receive_time_ms; // 받은 순간의 서버 시각. 틱 시각과 같은 시계이고, 처리할 틱을 기다린 시간이 섞이지 않는다.

	SessionPacketTask(SessionKey session_key, const char* packet_data, int packet_size, long long receive_time_ms)
		: session_key(session_key), packet(packet_data, packet_data + packet_size), receive_time_ms(receive_time_ms) {}
};
