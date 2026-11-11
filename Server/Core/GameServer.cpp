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
	accept_retry_over_.op_type = OPType::ACCEPT_RETRY;

	listen_socket_ = WSASocketW(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	memset(&server_addr_, 0, sizeof(server_addr_));
	server_addr_.sin_family = AF_INET;
	server_addr_.sin_port = htons(SERVER_PORT);
	server_addr_.sin_addr.S_un.S_addr = INADDR_ANY;

}

GameServer::~GameServer()
{
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
	if (!box_shape_data_.Load("Data/box_shapes.txt")) std::cerr << "Box shape data load fail.. " << std::endl;
	if (!box_candidate_data_.Load("Data/box_candidates.txt", "Data/building_boxes.txt", box_shape_data_)) std::cerr << "Box candidate data load fail.. " << std::endl;
	if (!map_region_data_.Load("Data/map_regions.txt")) std::cerr << "Map region data load fail.. " << std::endl;
	if (!map_region_data_.LoadTreasureExclusions("Data/treasure_exclusions.txt")) std::cerr << "Treasure exclusion data load fail.. " << std::endl;
	if (!hint_object_grid_.Build(map_data_)) std::cerr << "Hint object grid build fail.. " << std::endl;
	if (!direction_hint_data_.Build(map_region_data_)) std::cerr << "Direction hint data build fail.. " << std::endl;
	bind(listen_socket_, reinterpret_cast<sockaddr*>(&server_addr_), sizeof(server_addr_));
	listen(listen_socket_, SOMAXCONN);
	iocp_handle_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 0);
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(listen_socket_), iocp_handle_, LISTEN_IO_COMPLETION, 0);
	for (auto& accept_over : accept_overs_) PostAccept(accept_over);
}

// 완료된 수락 요청의 소켓을 세션에 넘기고, 같은 자리에서 다음 수락 요청을 건다.
// 새 세션은 로그인 대기로 시작한다. 중복 로그인 확인 범위는 로그인 DB 스레드만 바꾸므로 새 세션 작업으로 넣게 하고, 그 뒤에 수신을 건다.
// 받은 패킷과 연결 끊김은 수신·송신을 건 뒤에만 생기므로, 로그인 패킷과 정리 통보는 항상 새 세션 작업 뒤에 같은 큐로 들어간다.
void GameServer::CompleteAccept(AcceptOverlapped& accept_over)
{
	const SOCKET accept_socket = accept_over.accept_socket;
	auto* new_session = session_manager_->AcquireSession(accept_socket, iocp_handle_);
	if (new_session) {
		CreateIoCompletionPort(reinterpret_cast<HANDLE>(accept_socket), iocp_handle_, SESSION_IO_COMPLETION, 0);
		thread_manager_->GetDBThreadManager().GetLoginThread().Enqueue({ DBSessionTaskType::NEW_SESSION, new_session->GetSessionKey() });
		new_session->RecvPacket();
	}
	else {
		closesocket(accept_socket);
	}
	PostAccept(accept_over);
}

// 새 수락용 소켓을 만들어 이 자리의 OVERLAPPED로 수락 요청을 건다.
// 소켓을 만들지 못했거나 요청이 바로 실패하면 완료가 오지 않으므로, 다시 걸 자리로 표시해 두고 나중에 다시 건다.
void GameServer::PostAccept(AcceptOverlapped& accept_over)
{
	accept_over.accept_socket = WSASocketW(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (accept_over.accept_socket == INVALID_SOCKET) {
		std::cerr << "Accept socket create fail.. " << WSAGetLastError() << std::endl;
		MarkAcceptRetry(accept_over);
		return;
	}

	ZeroMemory(&accept_over.ex_over.over, sizeof(accept_over.ex_over.over));
	int addr_size = sizeof(SOCKADDR_IN);
	BOOL result = AcceptEx(listen_socket_, accept_over.accept_socket, accept_over.packet_buffer, 0, addr_size + 16, addr_size + 16, 0, &accept_over.ex_over.over);
	if (result || WSAGetLastError() == ERROR_IO_PENDING) return;
	std::cerr << "AcceptEx fail.. " << WSAGetLastError() << std::endl;
	closesocket(accept_over.accept_socket);
	accept_over.accept_socket = INVALID_SOCKET;
	MarkAcceptRetry(accept_over);
}

void GameServer::MarkAcceptRetry(AcceptOverlapped& accept_over)
{
	is_accept_retry_needed_[&accept_over - accept_overs_.data()].store(true);
}

// 수락 요청이 실패로 완료됐다. 받던 소켓은 닫고, 실패 사유에 따라 다시 건다.
// 서버 종료로 취소된 요청은 다시 걸지 않는다. 클라이언트가 접속 도중 끊은 것은 수락 자리에는 문제가 없으므로 바로 다시 건다.
// 그 밖의 사유(자원 부족 등)는 바로 다시 걸어도 같은 실패가 반복될 수 있으므로 정해진 간격 뒤에 다시 건다.
void GameServer::FailAccept(AcceptOverlapped& accept_over, DWORD error)
{
	if (accept_over.accept_socket != INVALID_SOCKET) closesocket(accept_over.accept_socket);
	accept_over.accept_socket = INVALID_SOCKET;
	if (error == ERROR_OPERATION_ABORTED || !IsRunning()) return;
	if (error == ERROR_NETNAME_DELETED || error == ERROR_CONNECTION_ABORTED || error == WSAECONNRESET || error == WSAECONNABORTED) {
		PostAccept(accept_over);
		return;
	}
	std::cerr << "Accept fail.. " << error << std::endl;
	MarkAcceptRetry(accept_over);
}

// 타이머가 수락 재시도 주기마다 호출한다. 다시 걸 자리가 있고 앞선 재시도가 끝났을 때만 넣는다.
void GameServer::PostAcceptRetry()
{
	bool has_retry = false;
	for (const auto& is_needed : is_accept_retry_needed_) has_retry = has_retry || is_needed.load();
	if (!has_retry || is_accept_retry_posted_.exchange(true)) return;
	PostQueuedCompletionStatus(iocp_handle_, 0, ACCEPT_RETRY_COMPLETION, &accept_retry_over_.over);
}

// 타이머가 넣은 완료를 받은 입출력 스레드가 다시 걸 자리마다 수락 요청을 건다. 또 실패하면 다음 주기에 다시 시도한다.
void GameServer::RetryFailedAccepts()
{
	for (int i = 0; i < ACCEPT_COUNT; ++i) {
		if (is_accept_retry_needed_[i].exchange(false)) PostAccept(accept_overs_[i]);
	}
	is_accept_retry_posted_.store(false);
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
	case ModeState::LOGIN: {
		DBSessionTask login_task;
		login_task.task_type = DBSessionTaskType::PACKET;
		login_task.session_key = task->session_key;
		login_task.request_time_ms = task->receive_time_ms;
		login_task.packet = std::move(task->packet);
		thread_manager_->GetDBThreadManager().GetLoginThread().Enqueue(std::move(login_task));
		break;
	}
	// 로비 전환 대기는 받을 곳이 로비로 정해져 있으므로 로비가 받는다. 처리할지는 로비가 정한다.
	case ModeState::LOBBY_PENDING:
	case ModeState::LOBBY:
		thread_manager_->GetLobbyThreadManager().EnqueueRequest(std::move(task));
		break;
	// 매칭 전환 대기는 받을 곳이 매칭으로 정해져 있으므로 매칭이 받는다. 처리할지는 매칭 스레드가 정한다.
	case ModeState::MATCHING_PENDING:
	case ModeState::MATCHING:
		thread_manager_->GetMatchingThreadManager().GetMatchingThread().EnqueueTask({ MatchingTaskType::PACKET, task->session_key, std::move(task->packet) });
		break;
	// 방 전환 대기는 요청을 보낸 쪽이 적은 목적지 방이 받는다. 방 번호가 없으면 방 생성 요청이므로 생성 처리가 받는다.
	case ModeState::ROOM_PENDING:
	case ModeState::ROOM: {
		auto* room = thread_manager_->GetGameThreadManager().GetRoomByIndex(session.GetRoomIndex());
		task->room_key = session.GetRoomKey();
		if (room) {
			room->AddPacketTask(std::move(task));
			break;
		}
		if (state.mode != ModeState::ROOM_PENDING) break;
		auto lifecycle_task = std::make_unique<RoomLifecycleTask>(RoomLifecycleTaskType::PACKET, task->session_key);
		lifecycle_task->packet = std::move(task->packet);
		EnqueueRoomLifecycleTask(std::move(lifecycle_task));
		break;
	}
	default:
		break;
	}
	return true;
}

// 연결 종료를 예약하고, 예약한 순간의 소속에 정리를 통보한다. 예약 뒤에는 소속이 바뀌지 않으므로 통보를 받은 곳이 최종 소속이고,
// 그곳이 자기 목록에서 빼고 정리 가능으로 바꿀 때까지 세션은 해제되지 않는다. 처음 예약한 한 번만 통보한다.
// 다른 곳으로 옮기는 중이었으면 통보하지 않는다. 옮기는 요청을 받은 곳이 확정하려다 예약을 보고 정리한다.
void GameServer::RequestDisconnect(SessionKey session_key)
{
	const ModeState mode = session_manager_->RequestDisconnect(session_key);
	auto& thread_manager = *thread_manager_;
	switch (mode) {
	case ModeState::LOGIN:
		thread_manager.GetDBThreadManager().GetLoginThread().Enqueue({ DBSessionTaskType::DISCONNECT, session_key });
		break;
	case ModeState::LOBBY:
		thread_manager.GetLobbyThreadManager().EnqueueTask({ LobbyTaskType::DISCONNECT, session_key });
		break;
	case ModeState::MATCHING:
		thread_manager.GetMatchingThreadManager().GetMatchingThread().EnqueueTask({ MatchingTaskType::DISCONNECT, session_key });
		break;
	case ModeState::ROOM: {
		// 방 번호와 방 키는 방이 입장을 확정하기 전에 적고, 방이 정리하기 전에는 바뀌지 않는다.
		auto* session = session_manager_->FindSession(session_key);
		auto* room = session ? thread_manager.GetGameThreadManager().GetRoomByIndex(session->GetRoomIndex()) : nullptr;
		if (!room) break;
		RoomTask task;
		task.task_type = RoomTaskType::DISCONNECT;
		task.session_key = session_key;
		task.room_key = session->GetRoomKey();
		room->AddRoomTask(std::move(task));
		break;
	}
	default:
		break;
	}
}

// 타이머가 정리 주기마다 호출한다. 앞선 정리가 끝나지 않았으면 넣지 않는다.
void GameServer::PostDisconnectCleanup()
{
	if (is_disconnect_cleanup_posted_.exchange(true)) return;
	PostQueuedCompletionStatus(iocp_handle_, 0, DISCONNECT_CLEANUP_COMPLETION, &disconnect_cleanup_over_.over);
}

// 연결 종료 후보를 훑어 정리 가능인 세션의 소켓을 닫고 해제한다. 타이머가 넣은 완료를 받은 입출력 스레드 하나가 실행한다.
// 세션 상태는 바꾸지 않는다. 소속 목록 정리는 각 소속이 하고, 끝나면 정리 가능으로 바꾼다.
void GameServer::ProcessDisconnects()
{
	auto& disconnect_list = session_manager_->GetDisconnectList();
	for (int list_index = 0; list_index < disconnect_list.GetEnd(); ++list_index) {
		const std::int32_t session_index = disconnect_list.GetValue(list_index);
		auto* session = session_manager_->FindSessionByIndex(session_index);
		if (!session || session->GetLifeState() != LifeState::DISCONNECT_READY) continue;
		session->CloseSocket();
		session_manager_->TryReleaseSession(*session);
	}
	is_disconnect_cleanup_posted_.store(false);
}

