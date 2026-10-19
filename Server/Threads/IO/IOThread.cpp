#include <chrono>
#include <iostream>
#include "IOThread.h"
#include "GameServer.h"
#include "SessionManager.h"
#include "common_packets.h"
#include "packet_types.h"
#include "packet_size_table.h"
#include "session_tasks.h"
#include "DBOverlapped.h"

IOThread::IOThread(GameServer& game_server)
	: game_server_(game_server)
{
}

void IOThread::Run()
{
	while (is_running_.load() && game_server_.IsRunning()) {
		DWORD transferred_bytes = 0;
		ULONG_PTR completion_key = 0;
		WSAOVERLAPPED* over = nullptr;
		BOOL result = GetQueuedCompletionStatus(game_server_.GetIOCPHandle(), &transferred_bytes, &completion_key, &over, INFINITE);
		if (over == nullptr) continue;

		ExOverlapped* ex_over = reinterpret_cast<ExOverlapped*>(over);
		switch (completion_key) {
		case LISTEN_IO_COMPLETION:
			ProcessAcceptCompletion(result, ex_over);
			break;

		case DB_SESSION_COMPLETION: {
			std::unique_ptr<DBOverlapped> db_over(reinterpret_cast<DBOverlapped*>(ex_over));
			game_server_.GetSessionManager().EnqueueSessionTask(db_over->ex_over.session_key, std::make_unique<SessionDBResultTask>(std::move(db_over->result)));
			break;
		}
		case SESSION_IO_COMPLETION:
			ProcessSessionCompletion(result, transferred_bytes, ex_over);
			break;



		default:
			break;
		}
	}
}

void IOThread::ProcessAcceptCompletion(BOOL result, ExOverlapped* ex_over)
{
	if (ex_over->op_type != OPType::ACCEPT) return;
	if (!result) {
		std::cout << "Accept Error" << WSAGetLastError() << "\n";
		return;
	}

	// 받은 주소는 수락 요청을 걸 때 넘긴 AcceptOverlapped의 주소이므로, 형식을 바꿔 그 요청의 소켓을 찾는다.
	game_server_.CompleteAccept(*static_cast<AcceptOverlapped*>(reinterpret_cast<IOOverlapped*>(ex_over)));
}

void IOThread::ProcessSessionCompletion(BOOL result, DWORD transferred_bytes, ExOverlapped* ex_over)
{
	const bool is_send = ex_over->op_type == OPType::SEND || ex_over->op_type == OPType::POOLED_SEND;
	IOOverlapped* io_over = reinterpret_cast<IOOverlapped*>(ex_over);
	SendBuffer* send_buffer = is_send ? static_cast<SendBuffer*>(io_over) : nullptr;

	switch (ex_over->op_type) {
	case OPType::RECV: {
		auto* session = game_server_.GetSessionManager().FindSession(ex_over->session_key);
		if (!session) return;
		ProcessReceiveCompletion(result, transferred_bytes, *session, ex_over->session_key);
		break;
	}

	case OPType::SEND:
	case OPType::POOLED_SEND:
		ProcessSendCompletion(result, transferred_bytes, send_buffer, ex_over->session_key);
		break;

	case OPType::LARGE_SEND: {
		const SessionKey session_key = ex_over->session_key;
		if (!result || transferred_bytes == 0) game_server_.GetSessionManager().RequestDisconnect(session_key);
		delete reinterpret_cast<LargeSendBuffer*>(ex_over);
		game_server_.GetSessionManager().CompleteSessionIO(session_key);
		break;
	}

	default:
		break;
	}
}

void IOThread::ProcessReceiveCompletion(BOOL result, DWORD transferred_bytes, Session& session, SessionKey session_key)
{
	if (!result || transferred_bytes == 0) {
		game_server_.GetSessionManager().RequestDisconnect(session_key);
		game_server_.GetSessionManager().CompleteSessionIO(session_key);
		return;
	}

	const bool should_receive = ProcessRecvBuffer(session, transferred_bytes);
	if (should_receive) session.RecvPacket();
	game_server_.GetSessionManager().CompleteSessionIO(session_key);
}

bool IOThread::ProcessRecvBuffer(Session& session, int recv_bytes)
{
	const SessionKey session_key = session.GetSessionKey();
	const int current_data_size = session.GetRemainingDataSize();
	if (recv_bytes < 0 || current_data_size < 0 || current_data_size > BUF_SIZE || recv_bytes > BUF_SIZE - current_data_size) {
		game_server_.GetSessionManager().RequestDisconnect(session_key);
		return false;
	}

	session.AdjustRemainingDataSize(recv_bytes);
	const int remaining_data_size = session.GetRemainingDataSize();
	char packet_buffer[BUF_SIZE];
	memcpy(packet_buffer, session.GetRecvOver().packet_buffer, remaining_data_size);

	const long long receive_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	int offset = 0;
	bool should_receive = true;
	while (remaining_data_size - offset >= PACKET_HEADER_SIZE) {
		char* packet = packet_buffer + offset;
		const auto* header = reinterpret_cast<const PACKET_HEADER*>(packet);
		const int packet_size = header->size;
		if (packet_size < PACKET_HEADER_SIZE || packet_size > BUF_SIZE) {
			game_server_.GetSessionManager().RequestDisconnect(session_key);
			should_receive = false;
			break;
		}
		// 처리 함수는 종류만 보고 구조체로 읽으므로, 종류별 허용 크기와 다르면 받은 범위를 넘어 읽게 된다. 정상 클라이언트가 만들 수 없는 패킷이라 연결을 끊는다.
		const PacketSizeRange& size_range = CLIENT_PACKET_SIZE_TABLE[header->type];
		if (packet_size < size_range.min_size || packet_size > size_range.max_size) {
			game_server_.GetSessionManager().RequestDisconnect(session_key);
			should_receive = false;
			break;
		}
		if (remaining_data_size - offset < packet_size) break;

		auto task = std::make_unique<SessionPacketTask>(packet, packet_size, receive_time_ms);
		if (!game_server_.GetSessionManager().EnqueueSessionTask(session_key, std::move(task))) {
			should_receive = false;
			break;
		}

		offset += packet_size;
	}

	session.AdjustRemainingDataSize(-offset);
	memmove(session.GetRecvOver().packet_buffer, session.GetRecvOver().packet_buffer + offset, session.GetRemainingDataSize());
	return should_receive;
}

void IOThread::ProcessSendCompletion(BOOL result, DWORD transferred_bytes, SendBuffer* send_buffer, SessionKey session_key)
{
	if (!result || transferred_bytes == 0) game_server_.GetSessionManager().RequestDisconnect(session_key);

	if (send_buffer->ex_over.op_type == OPType::POOLED_SEND) send_buffer->Clear();
	else delete send_buffer;
	game_server_.GetSessionManager().CompleteSessionIO(session_key);
}

void IOThread::Close()
{
	is_running_ = false;
	PostQueuedCompletionStatus(game_server_.GetIOCPHandle(), 0, 0, nullptr);
}
