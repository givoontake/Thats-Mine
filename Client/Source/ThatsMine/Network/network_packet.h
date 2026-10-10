#pragma once

#include "CoreMinimal.h"

namespace NetworkLimits
{
	// Server/Packets/common_packets.h와 Server/Common/settings.h의 통신 규격.
	inline constexpr int32 HEADER_SIZE = 3;
	inline constexpr int32 MAX_PACKET_SIZE = 2048;
	// 받는 패킷 하나의 최대 크기. 서버가 크기에 맞춘 버퍼로 따로 보내는 큰 패킷(상자 배치)까지 받도록 패킷 크기 칸의 최대값으로 둔다.
	inline constexpr int32 MAX_RECV_PACKET_SIZE = 65535;
	inline constexpr int32 MAX_QUEUED_PACKETS = 1024;
}

struct NetworkPacket
{
	uint8 type = 0;
	TArray<uint8> payload;
};
