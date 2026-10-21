#pragma once
#include <array>
#include <cstdint>
#include "settings.h"
#include "packet_types.h"
#include "common_packets.h"
#include "database_packets.h"
#include "room_packets.h"
#include "game_packets.h"

// 패킷 한 종류가 허용하는 전체 크기(헤더 포함) 범위
struct PacketSizeRange
{
	int min_size = 0;
	int max_size = 0;
};

// 클라이언트가 보내는 패킷 종류별 허용 크기. 종류 값을 그대로 인덱스로 쓴다.
// 처리 함수는 종류만 보고 구조체로 읽으므로, 수신 단계에서 이 표로 크기를 먼저 확인한다.
// 등록하지 않은 종류는 범위가 0으로 남아 어떤 크기도 통과하지 못한다.
inline constexpr std::array<PacketSizeRange, 256> CLIENT_PACKET_SIZE_TABLE = [] {
	std::array<PacketSizeRange, 256> table{};
	table[C2S_LOGIN] = { sizeof(C2S_LOGIN_PACKET), sizeof(C2S_LOGIN_PACKET) };
	table[C2S_ADD_PUBLIC_ROOM] = { sizeof(C2S_ADD_PUBLIC_ROOM_PACKET), sizeof(C2S_ADD_PUBLIC_ROOM_PACKET) };
	table[C2S_ADD_PRIVATE_ROOM] = { sizeof(C2S_ADD_PRIVATE_ROOM_PACKET), sizeof(C2S_ADD_PRIVATE_ROOM_PACKET) };
	table[C2S_JOIN_PUBLIC_ROOM] = { sizeof(C2S_JOIN_PUBLIC_ROOM_PACKET), sizeof(C2S_JOIN_PUBLIC_ROOM_PACKET) };
	table[C2S_JOIN_PRIVATE_ROOM] = { sizeof(C2S_JOIN_PRIVATE_ROOM_PACKET), sizeof(C2S_JOIN_PRIVATE_ROOM_PACKET) };
	table[C2S_REMOVE_PLAYER] = { sizeof(C2S_REMOVE_PLAYER_PACKET), sizeof(C2S_REMOVE_PLAYER_PACKET) };
	table[C2S_READY] = { sizeof(C2S_READY_PACKET), sizeof(C2S_READY_PACKET) };
	table[C2S_START] = { sizeof(C2S_START_PACKET), sizeof(C2S_START_PACKET) };
	table[C2S_KICK] = { sizeof(C2S_KICK_PACKET), sizeof(C2S_KICK_PACKET) };
	table[C2S_REQUEST_ROOM_LIST] = { sizeof(C2S_REQUEST_ROOM_LIST_PACKET), sizeof(C2S_REQUEST_ROOM_LIST_PACKET) };
	table[C2S_FAST_MATCHING] = { sizeof(C2S_FAST_MATCHING_PACKET), sizeof(C2S_FAST_MATCHING_PACKET) };
	table[C2S_MOVE] = { sizeof(C2S_MOVE_PACKET), sizeof(C2S_MOVE_PACKET) };
	table[C2S_MOVE_INPUT] = { sizeof(C2S_MOVE_INPUT_PACKET), sizeof(C2S_MOVE_INPUT_PACKET) };
	table[C2S_AIM] = { sizeof(C2S_AIM_PACKET), sizeof(C2S_AIM_PACKET) };
	table[C2S_EQUIP] = { sizeof(C2S_EQUIP_PACKET), sizeof(C2S_EQUIP_PACKET) };
	table[C2S_ACTION] = { sizeof(C2S_ACTION_PACKET), sizeof(C2S_ACTION_PACKET) };
	table[C2S_RELOAD] = { sizeof(C2S_RELOAD_PACKET), sizeof(C2S_RELOAD_PACKET) };
	table[C2S_LOAD_COMPLETE] = { sizeof(C2S_LOAD_COMPLETE_PACKET), sizeof(C2S_LOAD_COMPLETE_PACKET) };
	table[C2S_HIT] = { sizeof(C2S_HIT_PACKET), sizeof(C2S_HIT_PACKET) };
	table[C2S_MOVE_CORRECTION_ACK] = { sizeof(C2S_MOVE_CORRECTION_ACK_PACKET), sizeof(C2S_MOVE_CORRECTION_ACK_PACKET) };
	table[C2S_BOX_OPEN] = { sizeof(C2S_BOX_OPEN_PACKET), sizeof(C2S_BOX_OPEN_PACKET) };
	table[C2S_BOX_SHOT] = { sizeof(C2S_BOX_SHOT_PACKET), sizeof(C2S_BOX_SHOT_PACKET) };
	table[C2S_BLOCK_USE] = { sizeof(C2S_BLOCK_USE_PACKET), sizeof(C2S_BLOCK_USE_PACKET) };
	table[C2S_TREASURE_REVEAL] = { sizeof(C2S_TREASURE_REVEAL_PACKET), sizeof(C2S_TREASURE_REVEAL_PACKET) };
	table[C2S_AWARD_DONE] = { sizeof(C2S_AWARD_DONE_PACKET), sizeof(C2S_AWARD_DONE_PACKET) };
	return table;
}();
