#pragma once
#include <cstdint>

#pragma pack(push, 1)

struct PACKET_HEADER {
	std::uint16_t size;
	std::uint8_t type;
};

constexpr int PACKET_HEADER_SIZE = sizeof(PACKET_HEADER);

struct S2C_ERROR_PACKET {
	PACKET_HEADER header;
	int error_code;
};

struct C2S_DISCONNECT_PACKET {
	PACKET_HEADER header;
};

struct S2C_DISCONNECT_PACKET {
	PACKET_HEADER header;
	int player_id;
};

#pragma pack(pop)
