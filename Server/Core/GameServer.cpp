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
	for (int room_index = 0; room_index < MAX_ROOM_COUNT; ++room_index) rooms_[room_index].AttachToServer(this, room_index);

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

// 세션 작업 하나를 처리한다. room이 nullptr이면 로비가, 아니면 해당 방이 담당자로서 처리한다.
void GameServer::ProcessSessionTask(Session& session, std::unique_ptr<SessionTask> task, GameRoom* room)
{
	if (!task || !session.MatchesSessionKey(task->session_key)) return;
	switch (task->task_type) {
	case SessionTaskType::PACKET: {
		auto* packet_task = static_cast<SessionPacketTask*>(task.get());
		const std::uint8_t packet_type = reinterpret_cast<PACKET_HEADER*>(packet_task->packet.data())->type;
		if (packet_type == C2S_DISCONNECT) {
			session.BeginDisconnect(task->session_key);
			return;
		}
		RoutePacket(packet_task->packet.data(), session, room);
		break;
	}
	case SessionTaskType::DB_RESULT:
		DBResultHandler::HandleLogin(*this, session, static_cast<SessionDBResultTask*>(task.get())->result);
		break;
	case SessionTaskType::DISCONNECT:
		session.BeginDisconnect(task->session_key);
		break;
	}
}

// 담당자가 세션 작업을 처리한다. 작업마다 처리 후 세션 상태를 기대 상태와 비교해,
// 연결이 닫혔으면 남은 작업을 버리고, 담당자가 바뀌었으면 남은 작업 개수를 되돌려 다음 담당자가 이어서 처리하게 한다.
SessionDrainResult GameServer::DrainSessionTasks(Session& session, GameRoom* room)
{
	const auto is_owned = [room](ModeState mode_state) {
		if (room) return mode_state == ModeState::ROOM;
		return mode_state == ModeState::LOGIN || mode_state == ModeState::LOBBY || mode_state == ModeState::MATCHING;
	};

	const std::size_t task_count = session.ClaimTaskCount();
	for (std::size_t i = 0; i < task_count; ++i) {
		ProcessSessionTask(session, session.DequeueTask(), room);
		const SessionState state = session.GetState();
		if (state.life != LifeState::ACTIVE) {
			for (++i; i < task_count; ++i) session.DequeueTask();
			return SessionDrainResult::CLOSED;
		}
		if (!is_owned(state.mode)) {
			session.RestoreClaimedTaskCount(task_count - i - 1);
			return SessionDrainResult::HANDED_OVER;
		}
	}
	return SessionDrainResult::OWNED;
}

void GameServer::RoutePacket(char* packet, Session& session, GameRoom* room)
{
	if (room) room->HandlePacket(packet, session);
	else thread_manager_->GetLobbyThreadManager().ProcessPacket(packet, session);
}

Session* GameServer::AcquireSession(SOCKET new_socket)
{
	const std::uint64_t session_id = GenerateSessionID();
	if (session_id == 0) return nullptr;
	for (int i = 0; i < MAX_PLAYER_COUNT; ++i) {
		if (sessions_[i].InitSession(i, session_id, new_socket, iocp_handle_)) {
			const SessionKey session_key = sessions_[i].GetSessionKey();
			EnqueueLobbyTask(std::make_unique<LobbyTask>(LobbyTaskType::SESSION_ADDED, session_key));
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

// 세션 상태를 바꿔 로비 목록에 있어야 하는지가 달라진 쪽이 호출한다.
void GameServer::SyncLobbyList(int session_index)
{
	thread_manager_->GetLobbyThreadManager().SyncLobbyList(session_index);
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

// 방 자리를 반환한다. 자리가 비었는지, 어떤 방인지는 방 상태와 방 키로 확인한다.
GameRoom* GameServer::GetRoomByIndex(int room_index)
{
	if (room_index < 0 || room_index >= MAX_ROOM_COUNT) return nullptr;
	return &rooms_[room_index];
}

void GameServer::RequestDisconnect(SessionKey session_key)
{
	EnqueueSessionTask(session_key, std::make_unique<SessionDisconnectTask>());
}

void GameServer::CompleteSessionIO(SessionKey session_key)
{
	auto* session = FindSession(session_key);
	if (session && session->CompleteIO(session_key)) TryReleaseSession(*session);
}

// 연결이 닫힌 세션을 담당자가 자기 목록에서 정리한 뒤 호출한다. 담당 상태를 비우고 슬롯 정리를 시도한다.
void GameServer::ReleaseOwnership(Session& session)
{
	if (session.GetLifeState() != LifeState::CLOSED) return;
	session.ReleaseMode();
	TryReleaseSession(session);
}

// 담당자 정리와 마지막 입출력 완료 중 늦게 끝난 쪽이 슬롯을 정리한다.
void GameServer::TryReleaseSession(Session& session)
{
	if (!session.TryBeginRelease()) return;
	const SessionKey session_key = session.GetSessionKey();
	active_players_.RemovePlayer(session_key.player_id, session_key);
	session.DiscardTasks();
	session.CompleteRelease();
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

// 고정 길이 문자 배열을 복사한다. 첫 0 뒤의 값은 버리고 남는 부분을 0으로 채워, 배열 전체 비교가 정확하게 되도록 한다.
void GameServer::CopyCharBuf(char* dst, int dst_size, const char* src, int src_size)
{
	ZeroMemory(dst, dst_size);
	if (!src) return;
	const size_t copy_size = std::min(strnlen(src, static_cast<size_t>(src_size)), static_cast<size_t>(dst_size));
	memcpy(dst, src, copy_size);
}
