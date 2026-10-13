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

// 현재 담당자만 호출한다. 담당 상태는 유지한 채 연결 상태만 닫힘으로 바꾸고, 담당자는 이를 보고 자기 목록 정리를 진행한다.
bool Session::BeginDisconnect(SessionKey session_key)
{
	std::unique_lock<std::shared_mutex> socket_lock(socket_mutex_);
	std::lock_guard<std::mutex> session_lock(session_mutex_);
	if (!session_key_.IsSameSession(session_key)) return false;
	SessionState current_state = state_.load();
	while (current_state.life == LifeState::ACTIVE) {
		if (state_.compare_exchange_weak(current_state, { LifeState::CLOSED, current_state.mode })) break;
	}
	if (current_state.life != LifeState::ACTIVE) return false;
	const SOCKET socket = socket_;
	socket_ = INVALID_SOCKET;
	if (socket != INVALID_SOCKET) closesocket(socket);
	return true;
}

// 진행 중인 입출력이 모두 끝났으면 true를 반환한다.
bool Session::CompleteIO(SessionKey session_key)
{
	if (!MatchesSessionKey(session_key) || io_pending_count_.load() <= 0) return false;
	return --io_pending_count_ == 0;
}

// 담당자가 자기 목록 정리를 마친 뒤 담당 상태를 비운다. 연결이 닫힌 세션에만 사용한다.
void Session::ReleaseMode()
{
	SessionState current_state = state_.load();
	while (current_state.mode != ModeState::NONE) {
		if (state_.compare_exchange_weak(current_state, { current_state.life, ModeState::NONE })) break;
	}
}

// 연결이 닫혔고, 담당자가 없고, 진행 중인 입출력이 없을 때 한 스레드만 슬롯 정리를 시작한다.
// 담당자와 입출력 완료 스레드가 동시에 조건을 만족시킬 수 있으므로 CAS로 한쪽만 통과시킨다.
bool Session::TryBeginRelease()
{
	if (io_pending_count_.load() != 0) return false;
	SessionState expected_state{ LifeState::CLOSED, ModeState::NONE };
	return state_.compare_exchange_strong(expected_state, { LifeState::RELEASING, ModeState::NONE });
}

void Session::CompleteRelease()
{
	std::unique_lock<std::shared_mutex> socket_lock(socket_mutex_);
	std::lock_guard<std::mutex> session_lock(session_mutex_);
	if (GetLifeState() != LifeState::RELEASING) return;
	remaining_data_size_ = 0;
	session_key_.player_id = -1;
	session_key_.session_id = 0;
	nickname_.clear();
	ZeroMemory(&recv_over_.ex_over.over, sizeof(recv_over_.ex_over.over));
	ZeroMemory(recv_over_.packet_buffer, sizeof(recv_over_.packet_buffer));
	recv_over_.wsabuf.len = BUF_SIZE;
	recv_over_.wsabuf.buf = recv_over_.packet_buffer;
	recv_over_.ex_over.session_key = {};
	state_.store({ LifeState::NONE, ModeState::NONE });
}

bool Session::TryEnqueueTask(SessionKey session_key, std::unique_ptr<SessionTask> task)
{
	if (!task) return false;
	std::lock_guard<std::mutex> session_lock(session_mutex_);
	if (GetLifeState() != LifeState::ACTIVE) return false;
	if (!session_key_.IsSameSession(session_key)) return false;
	session_tasks_.Enqueue(std::move(task));
	return true;
}

void Session::DiscardTasks()
{
	const std::size_t task_count = session_tasks_.ClaimTaskCount();
	for (std::size_t i = 0; i < task_count; ++i) session_tasks_.Dequeue();
}

std::size_t Session::ClaimTaskCount()
{
	return session_tasks_.ClaimTaskCount();
}

void Session::RestoreClaimedTaskCount(std::size_t task_count)
{
	session_tasks_.RestoreClaimedTaskCount(task_count);
}

std::unique_ptr<SessionTask> Session::DequeueTask()
{
	return session_tasks_.Dequeue();
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

// 담당 상태가 기대한 값일 때만 바꾼다. 연결 상태는 담당자가 아닌 쪽에서 바뀔 수 있으므로 유지한 채 다시 시도한다.
bool Session::TryChangeMode(ModeState expected_mode, ModeState desired_mode)
{
	SessionState current_state = state_.load();
	while (current_state.mode == expected_mode) {
		if (state_.compare_exchange_weak(current_state, { current_state.life, desired_mode })) return true;
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

// 로그인 조회에 성공한 세션(AUTHENTICATING)에 플레이어 정보를 기록하고 로비 상태로 바꾼다.
bool Session::ApplyLoginResult(const DBLoginResult& result)
{
	std::lock_guard<std::mutex> lock(session_mutex_);
	if (GetLifeState() != LifeState::ACTIVE || GetModeState() != ModeState::AUTHENTICATING) return false;
	session_key_.player_id = result.player_id;
	nickname_ = result.nickname;
	return TryChangeMode(ModeState::AUTHENTICATING, ModeState::LOBBY);
}

std::string Session::GetNickname() const
{
	std::lock_guard<std::mutex> lock(session_mutex_);
	return nickname_;
}
