#include "GameRoom.h"
#include "Session.h"
#include "GameServer.h"
#include "packet_types.h"
#include "common_packets.h"

void GameRoom::Broadcast(char* packet)
{
	const int packet_size = static_cast<int>(reinterpret_cast<const PACKET_HEADER*>(packet)->size);
	auto& room_players = room_players_;
	for (auto& room_player : room_players) {
		if (!FindSession(room_player)) continue;
		GetTickSendBuffer(room_player).Append(packet, packet_size);
	}
}

void GameRoom::SendToPlaying(const char* packet, int packet_size)
{
	for (auto& room_player : room_players_) {
		if (FindSession(room_player) && room_player.GetRoomPlayerState() == RoomPlayerState::PLAY) GetTickSendBuffer(room_player).Append(packet, packet_size);
	}
}
