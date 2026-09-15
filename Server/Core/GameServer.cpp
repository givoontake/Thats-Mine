#include <algorithm>
#include <iostream>
#include "GameServer.h"
#include <Windows.h>
#include <bcrypt.h>
#include "ServerThreadManager.h"
#include "common_packets.h"
#include "lobby_tasks.h"
#include "packet_types.h"
#include "room_lifecycle_tasks.h"
#include "session_tasks.h"
#include "DBResultHandler.h"

#undef min

#pragma comment(lib, "bcrypt.lib")

GameServer::GameServer()
{
	WSAStartup(MAKEWORD(2, 2), &wsa_data_);

	listen_socket_ = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	accept_socket_ = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	memset(&server_addr_, 0, sizeof(server_addr_));
	server_addr_.sin_family = AF_INET;
	server_addr_.sin_port = htons(SERVER_PORT);
	server_addr_.sin_addr.S_un.S_addr = INADDR_ANY;

	accept_over_.SetOperationType(OPType::ACCEPT);

}

GameServer::~GameServer()
{
	active_players_.Clear();
	closesocket(listen_socket_);
	closesocket(accept_socket_);
	WSACleanup();
}

void GameServer::SendError(Session& session, int error_code)
{
	S2C_ERROR_PACKET error_p;
	error_p.header.size = static_cast<std::uint16_t>(sizeof(error_p));
	error_p.header.type = S2C_ERROR;
	error_p.error_code = error_code;

	session.SendPacket(reinterpret_cast<char*>(&error_p), error_p.header.size);
}

void GameServer::StartServer()
{
	bind(listen_socket_, reinterpret_cast<sockaddr*>(&server_addr_), sizeof(server_addr_));
	listen(listen_socket_, SOMAXCONN);
	iocp_handle_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(listen_socket_), iocp_handle_, LISTEN_IO_COMPLETION, 0);
	int addr_size = sizeof(SOCKADDR_IN);
	AcceptEx(listen_socket_, accept_socket_, accept_over_.packet_buffer, 0, addr_size + 16, addr_size + 16, 0, &accept_over_.ex_over.over);
}

void GameServer::CompleteAccept()
{
	auto* new_session = AcquireSession(accept_socket_);
	if (new_session) {
		CreateIoCompletionPort(reinterpret_cast<HANDLE>(accept_socket_), iocp_handle_, SESSION_IO_COMPLETION, 0);
		new_session->RecvPacket();
	}
	else {
		closesocket(accept_socket_);
	}
	accept_socket_ = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	if (accept_socket_ != INVALID_SOCKET) {
		ZeroMemory(&accept_over_.ex_over.over, sizeof(accept_over_.ex_over.over));
		int addr_size = sizeof(SOCKADDR_IN);
		BOOL result = AcceptEx(listen_socket_, accept_socket_, accept_over_.packet_buffer, 0, addr_size + 16, addr_size + 16, 0, &accept_over_.ex_over.over);
		if (!result && WSAGetLastError() != ERROR_IO_PENDING) std::cerr << "AcceptEx fail.. " << std::endl;
	}
}

void GameServer::StartThreads()
{
	thread_manager_ = std::make_unique<ServerThreadManager>(*this);
	thread_manager_->StartThreads();
}

void GameServer::CloseThreads()
{
	if (thread_manager_) thread_manager_->CloseThreads();
}

void GameServer::JoinThreads()
{
	if (thread_manager_) thread_manager_->JoinThreads();
}

bool GameServer::EnqueueSessionTask(SessionKey session_key, std::unique_ptr<SessionTask> task)
{
	if (!task) return false;
	auto* session = FindSession(session_key);
	if (!session) return false;
	task->session_key = session_key;
	return session->TryEnqueueTask(session_key, std::move(task));
}

void GameServer::EnqueueLobbyTask(std::unique_ptr<LobbyTask> task)
{
	thread_manager_->GetLobbyThreadManager().Enqueue(std::move(task));
}

void GameServer::EnqueueRoomLifecycleTask(std::unique_ptr<RoomLifecycleTask> task)
{
	thread_manager_->GetGameThreadManager().Enqueue(std::move(task));
}

SessionTaskProcessResult GameServer::ProcessSessionTask(Session& session, std::unique_ptr<SessionTask> task)
{
	if (!task || !session.MatchesSessionKey(task->session_key)) return SessionTaskProcessResult::CONTINUE;
	switch (task->task_type) {
	case SessionTaskType::PACKET: {
		auto* packet_task = static_cast<SessionPacketTask*>(task.get());
		const std::uint8_t packet_type = reinterpret_cast<PACKET_HEADER*>(packet_task->packet.data())->type;
		if (packet_type == C2S_DISCONNECT) {
			if (session.BeginDisconnect(task->session_key))
				EnqueueLobbyTask(std::make_unique<LobbyTask>(LobbyTaskType::DISCONNECT, task->session_key));
			return SessionTaskProcessResult::DISCARD_REMAINING;
		}
		RoutePacket(packet_task->packet.data(), session);
		if (packet_type == C2S_REMOVE_PLAYER) return SessionTaskProcessResult::RESTORE_REMAINING;
		break;
	}
	case SessionTaskType::DB_RESULT:
		DBResultHandler::HandleLogin(*this, session, static_cast<SessionDBResultTask*>(task.get())->result);
		break;
	case SessionTaskType::DISCONNECT:
		if (session.BeginDisconnect(task->session_key))
			EnqueueLobbyTask(std::make_unique<LobbyTask>(LobbyTaskType::DISCONNECT, task->session_key));
		return SessionTaskProcessResult::DISCARD_REMAINING;
	}
	return SessionTaskProcessResult::CONTINUE;
}

void GameServer::RoutePacket(char* packet, Session& session)
{
	auto room_snapshot = session.GetRoomSnapshot();
	switch (room_snapshot.mode_state) {
	case ModeState::NONE:
		return;
	case ModeState::LOGIN:
	case ModeState::LOBBY:
		thread_manager_->GetLobbyThreadManager().ProcessPacket(packet, session);
		break;
	case ModeState::ROOM:
		thread_manager_->GetGameThreadManager().ProcessPacket(packet, session);
		break;
	}

}

Session* GameServer::AcquireSession(SOCKET new_socket)
{
	const std::uint64_t session_id = GenerateSessionID();
	if (session_id == 0) return nullptr;
	for (int i = 0; i < MAX_PLAYER_COUNT; ++i) {
		if (sessions_[i].InitSession(i, session_id, new_socket, iocp_handle_)) {
			const SessionKey session_key = sessions_[i].GetSessionKey();
			EnqueueLobbyTask(std::make_unique<LobbyTask>(LobbyTaskType::ADD_SESSION, session_key));
			return &sessions_[i];
		}
	}
	return nullptr;
}

std::uint64_t GameServer::GenerateSessionID()
{
	std::uint64_t session_id = 0;
	while (session_id == 0) {
		if (BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(&session_id), sizeof(session_id), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0) return 0;
	}
	return session_id;
}

Session* GameServer::FindSessionByIndex(int session_index)
{
	if (session_index < 0 || session_index >= MAX_PLAYER_COUNT) return nullptr;
	auto* session = &sessions_[session_index];
	const LifeState life_state = session->GetLifeState();
	if (life_state == LifeState::NONE || life_state == LifeState::INITIALIZING) return nullptr;
	return session;
}

Session* GameServer::FindSession(SessionKey session_key)
{
	auto* session = FindSessionByIndex(session_key.session_index);
	if (!session || !session->MatchesSessionKey(session_key)) return nullptr;
	return session;
}

SP<GameRoom> GameServer::GetRoomByIndex(int room_index) const
{
	if (room_index < 0 || room_index >= MAX_ROOM_COUNT) return nullptr;
	return rooms_[room_index].load();
}

bool GameServer::TryAddRoom(int room_index, const SP<GameRoom>& room)
{
	if (room_index < 0 || room_index >= MAX_ROOM_COUNT || !room) return false;
	SP<GameRoom> expected_room;
	return std::atomic_compare_exchange_strong(&rooms_[room_index], &expected_room, room);
}

bool GameServer::TryRemoveRoom(int room_index, const SP<GameRoom>& room)
{
	if (room_index < 0 || room_index >= MAX_ROOM_COUNT || !room) return false;
	SP<GameRoom> expected_room = room;
	return std::atomic_compare_exchange_strong(&rooms_[room_index], &expected_room, SP<GameRoom>{});
}

void GameServer::RequestDisconnect(SessionKey session_key)
{
	EnqueueSessionTask(session_key, std::make_unique<SessionDisconnectTask>());
}

void GameServer::CompleteSessionIO(SessionKey session_key)
{
	auto* session = FindSession(session_key);
	if (session && session->CompleteIO(session_key))
		EnqueueLobbyTask(std::make_unique<LobbyTask>(LobbyTaskType::DISCONNECT, session_key));
}

void GameServer::CompleteRoomDisconnect(SessionKey session_key)
{
	auto* session = FindSession(session_key);
	if (session && session->CompleteRoomRemoval(session_key))
		EnqueueLobbyTask(std::make_unique<LobbyTask>(LobbyTaskType::DISCONNECT, session_key));
}

bool GameServer::FinalizeDisconnect(SessionKey session_key)
{
	auto* session = FindSession(session_key);
	if (!session || session->GetLifeState() != LifeState::DISCONNECTING) return true;
	if (!session->TryStartTaskProcessing()) return false;
	if (!session->MatchesSessionKey(session_key) || session->GetLifeState() != LifeState::DISCONNECTING) {
		session->CompleteTaskProcessing();
		return true;
	}

	const SessionKey current_session_key = session->GetSessionKey();
	const RoomSnapshot room_snapshot = session->GetRoomSnapshot();
	if (room_snapshot.mode_state == ModeState::ROOM) {
		auto room = GetRoomByIndex(room_snapshot.room_index);
		if (room) {
			RoomTask task;
			task.task_type = RoomTaskType::REMOVE_PLAYER;
			task.session_key = current_session_key;
			const bool is_enqueued = room->AddRoomTask(std::move(task));
			session->CompleteTaskProcessing();
			return is_enqueued;
		}
		CompleteRoomDisconnect(current_session_key);
		session->CompleteTaskProcessing();
		return true;
	}

	active_players_.RemovePlayer(current_session_key.player_id, current_session_key);
	auto& lobby_thread_manager = thread_manager_->GetLobbyThreadManager();
	auto* lobby_session = lobby_thread_manager.GetLobbySession(current_session_key.session_index);
	if (lobby_session && lobby_session->Clear(current_session_key)) lobby_thread_manager.RemoveActiveSession(current_session_key);
	session->DiscardTasks();
	session->CompleteTaskProcessing();
	session->FinalizeDisconnect(current_session_key);
	return true;
}

void GameServer::StringToCharBuf(const std::string& str, char* buf, int buf_size)
{
	ZeroMemory(buf, buf_size);
	size_t copy_size = std::min(str.size(), static_cast<size_t>(buf_size));
	memcpy(buf, str.data(), copy_size);
}

std::string GameServer::CharBufToString(const char* buf, int buf_size)
{
	size_t copy_size = strnlen(buf, static_cast<size_t>(buf_size));
	std::string str(buf, copy_size);
	return str;
}
