#include <iostream>
#include "GameServer.h"
#include "GameRoom.h"
#include "ServerThreadManager.h"
#include "SessionManager.h"
#include "common_packets.h"
#include "packet_types.h"
#include "room_lifecycle_tasks.h"
#include "session_tasks.h"

GameServer::GameServer()
{
	WSAStartup(MAKEWORD(2, 2), &wsa_data_);
	session_manager_ = std::make_unique<SessionManager>();
	disconnect_cleanup_over_.op_type = OPType::DISCONNECT_CLEANUP;

	listen_socket_ = WSASocketW(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	memset(&server_addr_, 0, sizeof(server_addr_));
	server_addr_.sin_family = AF_INET;
	server_addr_.sin_port = htons(SERVER_PORT);
	server_addr_.sin_addr.S_un.S_addr = INADDR_ANY;

}

GameServer::~GameServer()
{
	session_manager_->GetActivePlayerManager().Clear();
	closesocket(listen_socket_);
	for (auto& accept_over : accept_overs_) {
		if (accept_over.accept_socket != INVALID_SOCKET) closesocket(accept_over.accept_socket);
	}
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
	if (!hitbox_data_.Load("Data/hitbox_data.bin")) std::cerr << "Hitbox data load fail.. " << std::endl;
	if (!map_data_.Load("Data/Map")) std::cerr << "Map data load fail.. " << std::endl;
	if (!box_candidate_data_.Load("Data/box_candidates.txt", "Data/building_boxes.txt")) std::cerr << "Box candidate data load fail.. " << std::endl;
	if (!map_region_data_.Load("Data/map_regions.txt")) std::cerr << "Map region data load fail.. " << std::endl;
	if (!map_region_data_.LoadTreasureExclusions("Data/treasure_exclusions.txt")) std::cerr << "Treasure exclusion data load fail.. " << std::endl;
	if (!hint_object_tree_.Build(map_data_)) std::cerr << "Hint object tree build fail.. " << std::endl;
	if (!direction_hint_data_.Build(map_region_data_)) std::cerr << "Direction hint data build fail.. " << std::endl;
	bind(listen_socket_, reinterpret_cast<sockaddr*>(&server_addr_), sizeof(server_addr_));
	listen(listen_socket_, SOMAXCONN);
	iocp_handle_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(listen_socket_), iocp_handle_, LISTEN_IO_COMPLETION, 0);
	for (auto& accept_over : accept_overs_) PostAccept(accept_over);
}

// 완료된 수락 요청의 소켓을 세션에 넘기고, 같은 자리에서 다음 수락 요청을 건다.
void GameServer::CompleteAccept(AcceptOverlapped& accept_over)
{
	const SOCKET accept_socket = accept_over.accept_socket;
	auto* new_session = session_manager_->AcquireSession(accept_socket, iocp_handle_);
	if (new_session) {
		CreateIoCompletionPort(reinterpret_cast<HANDLE>(accept_socket), iocp_handle_, SESSION_IO_COMPLETION, 0);
		// 세션 관리는 로그인 처리를 알지 않으므로, 새 세션을 로그인 대기 목록에 넣는 것은 세션을 받은 서버가 한다.
		SyncLoginList(new_session->GetSessionKey().session_index);
		new_session->RecvPacket();
	}
	else {
		closesocket(accept_socket);
	}
	PostAccept(accept_over);
}

// 새 수락용 소켓을 만들어 이 자리의 OVERLAPPED로 수락 요청을 건다.
void GameServer::PostAccept(AcceptOverlapped& accept_over)
{
	accept_over.accept_socket = WSASocketW(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (accept_over.accept_socket == INVALID_SOCKET) return;

	ZeroMemory(&accept_over.ex_over.over, sizeof(accept_over.ex_over.over));
	int addr_size = sizeof(SOCKADDR_IN);
	BOOL result = AcceptEx(listen_socket_, accept_over.accept_socket, accept_over.packet_buffer, 0, addr_size + 16, addr_size + 16, 0, &accept_over.ex_over.over);
	if (!result && WSAGetLastError() != ERROR_IO_PENDING) std::cerr << "AcceptEx fail.. " << std::endl;
}

void GameServer::StartThreads()
{
	thread_manager_ = std::make_unique<ServerThreadManager>(*this, TickWaitPolicy::FULL_SLEEP);
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

void GameServer::EnqueueRoomLifecycleTask(std::unique_ptr<RoomLifecycleTask> task)
{
	thread_manager_->GetGameThreadManager().Enqueue(std::move(task));
}

// 받은 패킷을 받을 때의 세션 상태로 정한 처리 단위의 큐에 넣는다. 처리하는 쪽이 지금도 자기 소속인지 다시 확인하므로,
// 넣은 뒤에 상태가 바뀌어도 잘못 처리되지 않는다. 연결이 활성이 아니면 false를 반환해 수신을 멈추게 한다.
bool GameServer::RoutePacket(Session& session, std::unique_ptr<SessionPacketTask> task)
{
	const SessionState state = session.GetState();
	if (state.life != LifeState::ACTIVE) return false;
	switch (state.mode) {
	case ModeState::LOGIN:
		thread_manager_->GetDBThreadManager().GetLoginThread().EnqueueLoginPacket(task->session_key, task->packet.data(), task->receive_time_ms);
		break;
	// 방 전환 대기 중인 세션도 아직 로비 목록에 있으므로 로비가 받는다. 상태를 바꾸는 요청은 로비 상태가 아니라 거절된다.
	case ModeState::LOBBY:
	case ModeState::MATCHING:
	case ModeState::ROOM_PENDING:
		thread_manager_->GetLobbyThreadManager().EnqueueRequest(std::move(task));
		break;
	case ModeState::ROOM: {
		auto* room = thread_manager_->GetGameThreadManager().GetRoomByIndex(session.GetRoomIndex());
		if (room) room->AddPacketTask(std::move(task));
		break;
	}
	default:
		break;
	}
	return true;
}

// 연결 종료를 예약한다. 방에 있던 세션이면 그 방에 바로 연결 끊김 작업을 넣어 다음 틱에 자리가 빠지게 한다.
// 종료가 예약된 세션의 방 소속은 방이 자리를 뺄 때까지 바뀌지 않고, 예약 뒤에 방 소속이 되는 일도 없으므로 이 한 번으로 충분하다.
void GameServer::RequestDisconnect(SessionKey session_key)
{
	if (!session_manager_->RequestDisconnect(session_key)) return;
	auto* session = session_manager_->FindSession(session_key);
	if (!session || session->GetModeState() != ModeState::ROOM) return;
	auto* room = thread_manager_->GetGameThreadManager().GetRoomByIndex(session->GetRoomIndex());
	if (!room) return;
	RoomTask task;
	task.task_type = RoomTaskType::DISCONNECT;
	task.session_key = session_key;
	task.room_key = room->GetRoomKey();
	room->AddRoomTask(std::move(task));
}

// 타이머가 정리 주기마다 호출한다. 앞선 정리가 끝나지 않았으면 넣지 않는다.
void GameServer::PostDisconnectCleanup()
{
	if (is_disconnect_cleanup_posted_.exchange(true)) return;
	PostQueuedCompletionStatus(iocp_handle_, 0, DISCONNECT_CLEANUP_COMPLETION, &disconnect_cleanup_over_.over);
}

// 연결 종료 후보를 훑어 정리한다. 타이머가 넣은 완료를 받은 입출력 스레드 하나가 실행한다.
// 소속 목록(로그인 대기, 로비, 매칭)의 정리와 소켓 닫기, 세션 해제는 모두 여기서만 한다. 방의 플레이어 자리만 방이 직접 뺀다.
// 소속이 없음이 될 때까지는 정리하지 않고, 없음이 되면 목록에서 뺀 뒤 소켓을 닫고 해제한다.
void GameServer::ProcessDisconnects()
{
	auto& disconnect_list = session_manager_->GetDisconnectList();
	for (int list_index = 0; list_index < disconnect_list.GetEnd(); ++list_index) {
		const std::int32_t session_index = disconnect_list.GetValue(list_index);
		auto* session = session_manager_->FindSessionByIndex(session_index);
		if (!session) continue;
		const SessionState state = session->GetState();
		if (state.life != LifeState::DISCONNECTING) continue;
		switch (state.mode) {
		// 전환을 시도하는 쪽이 없는 로그인·로비 세션은 정리 단계가 직접 소속을 놓는다.
		case ModeState::LOGIN:
		case ModeState::LOBBY:
		case ModeState::MATCHING:
			session->ReleaseMode(state.mode);
			break;
		// 방 소속은 종료를 예약할 때 넣은 연결 끊김 작업으로 방이 자리를 빼고 소속을 놓는다.
		// 방 전환 대기는 요청을 받은 방이 결과를 확정하려다 종료 예약을 보고 소속을 놓는다.
		default:
			break;
		}
		if (session->GetModeState() != ModeState::NONE) continue;

		SyncLoginList(session_index);
		SyncLobbyList(session_index);
		thread_manager_->GetLobbyThreadManager().GetMatchmaker().Remove(session_index);
		session->CloseSocket();
		session_manager_->TryReleaseSession(*session);
	}
	is_disconnect_cleanup_posted_.store(false);
}

// 세션 상태를 바꿔 로그인 대기 목록에 있어야 하는지가 달라진 쪽이 호출한다.
void GameServer::SyncLoginList(int session_index)
{
	thread_manager_->GetDBThreadManager().GetLoginThread().GetLoginManager().SyncLoginList(session_index);
}

// 세션 상태를 바꿔 로비 목록에 있어야 하는지가 달라진 쪽이 호출한다.
void GameServer::SyncLobbyList(int session_index)
{
	thread_manager_->GetLobbyThreadManager().SyncLobbyList(session_index);
}
