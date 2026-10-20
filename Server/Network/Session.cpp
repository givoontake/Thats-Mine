#include <iostream>
#include <mutex>
#include "Session.h"

Session::Session()
{
	recv_over_.SetOperationType(OPType::RECV);
}

bool Session::InitSession(int session_index, std::uint64_t session_id, SOCKET new_socket, HANDLE iocp_handle)
{
	if (session_index < 0 || session_id == 0 || new_socket == INVALID_SOCKET || !iocp_handle) return false;
	SessionState expected_state{ LifeState::NONE, ModeState::NONE };
	if (!state_.compare_exchange_strong(expected_state, { LifeState::INITIALIZING, ModeState::NONE })) return false;

	std::unique_lock<std::shared_mutex> socket_lock(socket_mutex_);
	std::lock_guard<std::mutex> session_lock(session_mutex_);
	socket_ = new_socket;
	iocp_handle_ = iocp_handle;
	io_pending_count_ = 0;
	remaining_data_size_ = 0;
	room_index_.store(-1);
	session_key_.session_index = session_index;
	session_key_.player_id = -1;
	nickname_.clear();
	session_key_.session_id = session_id;
	ZeroMemory(&recv_over_.ex_over.over, sizeof(recv_over_.ex_over.over));
	ZeroMemory(recv_over_.packet_buffer, sizeof(recv_over_.packet_buffer));
	recv_over_.wsabuf.len = BUF_SIZE;
	recv_over_.wsabuf.buf = recv_over_.packet_buffer;
	recv_over_.ex_over.session_key = session_key_;
	state_.store({ LifeState::ACTIVE, ModeState::LOGIN });
	return true;
}

bool Session::SendPacket(const char* packet, int packet_size)
{
	if (!packet || packet_size <= 0 || packet_size > BUF_SIZE) return false;
	const int buffer_index = send_buffer_pool_.FindAvailableBuffer();
	const bool is_pooled = buffer_index >= 0;
	SendBuffer* send_buffer = is_pooled ? &send_buffer_pool_.send_buffers[buffer_index] : new SendBuffer;
	if (!send_buffer->Append(packet, packet_size)) {
		if (is_pooled) send_buffer->Clear();
		else delete send_buffer;
		return false;
	}

	std::shared_lock<std::shared_mutex> socket_lock(socket_mutex_);
	if (GetLifeState() != LifeState::ACTIVE || socket_ == INVALID_SOCKET) {
		if (is_pooled) send_buffer->Clear();
		else delete send_buffer;
		return false;
	}

	send_buffer->ex_over.session_key = GetSessionKey();
	++io_pending_count_;
	std::unique_lock<std::mutex> send_lock(send_mutex_);
	const int result = WSASend(socket_, &send_buffer->wsabuf, 1, 0, 0, &send_buffer->ex_over.over, 0);
	const int error = result == SOCKET_ERROR ? WSAGetLastError() : 0;
	send_lock.unlock();
	if (result == SOCKET_ERROR && error != WSA_IO_PENDING) {
		PostQueuedCompletionStatus(iocp_handle_, 0, SESSION_IO_COMPLETION, &send_buffer->ex_over.over);
		std::cerr << "Session::SendPacket() WSASend error\n";
		return false;
	}
	return true;
}

bool Session::SendLargePacket(const char* packet, int packet_size)
{
	if (!packet || packet_size <= 0) return false;
	LargeSendBuffer* send_buffer = new LargeSendBuffer(packet, packet_size);

	std::shared_lock<std::shared_mutex> socket_lock(socket_mutex_);
	if (GetLifeState() != LifeState::ACTIVE || socket_ == INVALID_SOCKET) {
		delete send_buffer;
		return false;
	}

	send_buffer->ex_over.session_key = GetSessionKey();
	++io_pending_count_;
	std::unique_lock<std::mutex> send_lock(send_mutex_);
	const int result = WSASend(socket_, &send_buffer->wsabuf, 1, 0, 0, &send_buffer->ex_over.over, 0);
	const int error = result == SOCKET_ERROR ? WSAGetLastError() : 0;
	send_lock.unlock();
	if (result == SOCKET_ERROR && error != WSA_IO_PENDING) {
		PostQueuedCompletionStatus(iocp_handle_, 0, SESSION_IO_COMPLETION, &send_buffer->ex_over.over);
		std::cerr << "Session::SendLargePacket() WSASend error\n";
		return false;
	}
	return true;
}

bool Session::RecvPacket()
{
	std::shared_lock<std::shared_mutex> socket_lock(socket_mutex_);
	if (GetLifeState() != LifeState::ACTIVE || socket_ == INVALID_SOCKET) return false;

	DWORD recv_flag = 0;
	ZeroMemory(&recv_over_.ex_over.over, sizeof(recv_over_.ex_over.over));
	recv_over_.ex_over.session_key = GetSessionKey();
	const int remaining_data_size = GetRemainingDataSize();
	recv_over_.wsabuf.len = BUF_SIZE - remaining_data_size;
	recv_over_.wsabuf.buf = recv_over_.packet_buffer + remaining_data_size;
	++io_pending_count_;
	const int result = WSARecv(socket_, &recv_over_.wsabuf, 1, 0, &recv_flag, &recv_over_.ex_over.over, 0);
	if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
		PostQueuedCompletionStatus(iocp_handle_, 0, SESSION_IO_COMPLETION, reinterpret_cast<WSAOVERLAPPED*>(&recv_over_));
		std::cerr << "Session::RecvPacket() WSARecv error\n";
		return false;
	}
	return true;
}

bool Session::MatchesSessionKey(SessionKey session_key) const
{
	const LifeState life_state = GetLifeState();
	if (life_state == LifeState::NONE || life_state == LifeState::INITIALIZING) return false;
	std::lock_guard<std::mutex> lock(session_mutex_);
	return session_key_.IsSameSession(session_key);
}

void Session::AdjustRemainingDataSize(int data_size_delta)
{
	std::lock_guard<std::mutex> lock(session_mutex_);
	remaining_data_size_ += data_size_delta;
}

// 소속은 유지한 채 연결 상태만 종료 예약으로 바꾼다. 소켓은 정리가 끝난 뒤 정리 단계가 닫는다.
// 이미 예약됐거나 다른 연결로 재사용된 세션이면 false를 반환한다.
bool Session::BeginDisconnect(SessionKey session_key)
{
	std::lock_guard<std::mutex> session_lock(session_mutex_);
	if (!session_key_.IsSameSession(session_key)) return false;
	SessionState current_state = state_.load();
	while (current_state.life == LifeState::ACTIVE) {
		if (state_.compare_exchange_weak(current_state, { LifeState::DISCONNECTING, current_state.mode })) return true;
	}
	return false;
}

// 입출력 하나가 끝났음을 센다. 세션 해제는 정리 단계가 남은 입출력이 0인 것을 보고 한다.
void Session::CompleteIO(SessionKey session_key)
{
	if (!MatchesSessionKey(session_key) || io_pending_count_.load() <= 0) return;
	--io_pending_count_;
}

// 종료 예약된 세션의 소속을 기대한 값에서 없음으로 바꾼다. 소속된 곳이 자기 목록 정리를 마쳤다는 표시다.
bool Session::ReleaseMode(ModeState expected_mode)
{
	SessionState expected_state{ LifeState::DISCONNECTING, expected_mode };
	return state_.compare_exchange_strong(expected_state, { LifeState::DISCONNECTING, ModeState::NONE });
}

// 송수신 등록과 겹치지 않도록 단독 잠금으로 닫는다. 닫으면 걸려 있던 입출력이 실패로 완료되어 남은 입출력 수가 줄어든다.
void Session::CloseSocket()
{
	std::unique_lock<std::shared_mutex> socket_lock(socket_mutex_);
	const SOCKET socket = socket_;
	socket_ = INVALID_SOCKET;
	if (socket != INVALID_SOCKET) closesocket(socket);
}

// 소속된 곳이 정리를 마쳤고(소속 없음), 소켓을 닫았고, 남은 입출력이 없을 때 세션 자리를 비운다.
// 정리 단계 하나만 호출하므로 해제를 두고 경쟁하는 쪽이 없다. 해제 전까지 연결 상태가 종료 예약이라 새 접속도 이 자리를 차지하지 못한다.
bool Session::TryRelease()
{
	std::unique_lock<std::shared_mutex> socket_lock(socket_mutex_);
	std::lock_guard<std::mutex> session_lock(session_mutex_);
	const SessionState state = state_.load();
	if (state.life != LifeState::DISCONNECTING || state.mode != ModeState::NONE) return false;
	if (socket_ != INVALID_SOCKET || io_pending_count_.load() != 0) return false;
	remaining_data_size_ = 0;
	room_index_.store(-1);
	session_key_.player_id = -1;
	session_key_.session_id = 0;
	nickname_.clear();
	ZeroMemory(&recv_over_.ex_over.over, sizeof(recv_over_.ex_over.over));
	ZeroMemory(recv_over_.packet_buffer, sizeof(recv_over_.packet_buffer));
	recv_over_.wsabuf.len = BUF_SIZE;
	recv_over_.wsabuf.buf = recv_over_.packet_buffer;
	recv_over_.ex_over.session_key = {};
	state_.store({ LifeState::NONE, ModeState::NONE });
	return true;
}

SessionKey Session::GetSessionKey() const
{
	std::lock_guard<std::mutex> lock(session_mutex_);
	return session_key_;
}

int Session::GetRemainingDataSize() const
{
	std::lock_guard<std::mutex> lock(session_mutex_);
	return remaining_data_size_;
}

// 소속이 기대한 값일 때만 바꾼다. 연결 상태는 소속된 곳이 아닌 쪽에서 바뀔 수 있으므로 다시 읽어 판단한다.
// 연결 종료가 예약된 세션이면 바꾸려던 쪽이 원래 전환 대신 소속을 없음으로 만들고 false를 반환한다. 목록 정리는 정리 단계가 한다.
// 방 소속은 방이 플레이어 자리를 직접 빼야 하므로 바꾸지 않고 false만 반환하며, 방이 자리를 뺀 뒤 ReleaseMode로 놓는다.
bool Session::TryChangeMode(ModeState expected_mode, ModeState desired_mode)
{
	SessionState current_state = state_.load();
	while (current_state.mode == expected_mode) {
		if (current_state.life == LifeState::ACTIVE) {
			if (state_.compare_exchange_weak(current_state, { LifeState::ACTIVE, desired_mode })) return true;
			continue;
		}
		if (current_state.life != LifeState::DISCONNECTING || expected_mode == ModeState::ROOM) return false;
		if (state_.compare_exchange_weak(current_state, { LifeState::DISCONNECTING, ModeState::NONE })) return false;
	}
	return false;
}

// 세션 키가 같을 때만 모드를 바꾼다. 세션을 처리하지 않는 스레드가 키로 찾은 세션을 바꿀 때 쓴다.
// 세션을 비우거나 새 접속에 배정하는 쪽도 같은 잠금 안에서 키를 바꾸므로, 찾은 뒤 다른 연결로 재사용된 세션은 바꾸지 않는다.
bool Session::TryChangeMode(SessionKey session_key, ModeState expected_mode, ModeState desired_mode)
{
	std::lock_guard<std::mutex> lock(session_mutex_);
	if (!session_key_.IsSameSession(session_key)) return false;
	return TryChangeMode(expected_mode, desired_mode);
}

// 로그인 조회에 성공한 세션(LOGIN)에 플레이어 정보를 기록하고 로비 상태로 바꾼다.
bool Session::ApplyLoginResult(const DBLoginResult& result)
{
	std::lock_guard<std::mutex> lock(session_mutex_);
	if (GetLifeState() != LifeState::ACTIVE || GetModeState() != ModeState::LOGIN) return false;
	session_key_.player_id = result.player_id;
	nickname_ = result.nickname;
	return TryChangeMode(ModeState::LOGIN, ModeState::LOBBY);
}

std::string Session::GetNickname() const
{
	std::lock_guard<std::mutex> lock(session_mutex_);
	return nickname_;
}
