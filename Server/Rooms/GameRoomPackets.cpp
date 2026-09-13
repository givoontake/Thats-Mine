#include <cstring>
#include "GameRoom.h"
#include "GameServer.h"
#include "packet_types.h"
#include "common_packets.h"

void GameRoom::BroadcastPackets()
{
	auto& room_players = room_players_;
	char send_buffer[BUF_SIZE];
	int send_data_size = 0;

	for (int i = 0; i < room_players.size(); ++i) {
		Player& source = room_players[i];
		auto source_session = FindSession(source);
		if (!source_session) continue;
		const int source_data_size = source.GetSendDataSize();
		if (source_data_size <= 0) continue;

		if (send_data_size + source_data_size > BUF_SIZE) {
			for (auto& target : room_players) {
				auto target_session = FindSession(target);
				if (!target_session) continue;
				target_session->SendPacket(send_buffer, send_data_size);
			}
			send_data_size = 0;
		}

		memcpy(send_buffer + send_data_size, source.GetSendBuffer(), source_data_size);
		send_data_size += source_data_size;
	}

	if (send_data_size > 0) {
		for (auto& target : room_players) {
			auto target_session = FindSession(target);
			if (!target_session) continue;
			target_session->SendPacket(send_buffer, send_data_size);
		}
	}

	for (auto& room_player : room_players) {
		auto session = FindSession(room_player);
		if (!session) continue;
		room_player.ClearSendBuffer();
	}
}

//void GameRoom::BroadcastPackets()
//{
//	auto& room_players = room_players_;
//	for (auto& source : room_players) {
//		auto source_session = FindSession(source);
//		if (!source_session) continue;
//		const int source_data_size = source.GetSendDataSize();
//		if (source_data_size <= 0) continue;
//
//		for (auto& target : room_players) {
//			auto target_session = FindSession(target);
//			if (!target_session) continue;
//			target_session->SendPacket(source.GetSendBuffer(), source_data_size);
//		}
//	}
//
//	for (auto& room_player : room_players) {
//		auto session = FindSession(room_player);
//		if (!session) continue;
//		room_player.ClearSendBuffer();
//	}
//}

void GameRoom::Broadcast(char* packet)
{
	std::vector<int> target_index;
	const int packet_size = static_cast<int>(reinterpret_cast<const PACKET_HEADER*>(packet)->size);
	auto& room_players = room_players_;
	for (auto& room_player : room_players) {
		auto session = FindSession(room_player);
		if (!session) continue;
		session->SendPacket(packet, packet_size);
	}
}
