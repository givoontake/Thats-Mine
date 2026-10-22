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
		auto session = FindSession(room_player);
		if (!session) continue;
		session->SendPacket(packet, packet_size);
	}
}

void GameRoom::SendToPlaying(const char* packet, int packet_size)
{
	for (auto& room_player : room_players_) {
		auto session = FindSession(room_player);
		if (session && room_player.GetRoomPlayerState() == RoomPlayerState::PLAY) session->SendPacket(packet, packet_size);
	}
}
