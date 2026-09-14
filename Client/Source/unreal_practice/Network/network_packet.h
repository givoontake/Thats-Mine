#pragma once

#include "CoreMinimal.h"

namespace NetworkLimits
{
	// Server/Packets/common_packets.h와 Server/Common/settings.h의 통신 규격.
	inline constexpr int32 HEADER_SIZE = 3;
	inline constexpr int32 MAX_PACKET_SIZE = 2048;
	inline constexpr int32 MAX_QUEUED_PACKETS = 1024;
}

struct NetworkPacket
{
	uint8 type = 0;
	TArray<uint8> payload;
};
