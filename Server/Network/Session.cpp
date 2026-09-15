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
	LifeState expected_state = LifeState::NONE;
	if (!life_state_.compare_exchange_strong(expected_state, LifeState::INITIALIZING)) return false;

	std::unique_lock<std::shared_mutex> socket_lock(socket_mutex_);
	std::lock_guard<std::mutex> session_lock(session_mutex_);
	socket_ = new_socket;
	iocp_handle_ = iocp_handle;
	io_pending_count_ = 0;
	room_index_ = -1;
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
	life_state_.store(LifeState::ACTIVE);
	mode_state_.store(ModeState::LOGIN);
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
	if (life_state_.load() != LifeState::ACTIVE || socket_ == INVALID_SOCKET) {
		if (is_pooled) send_buffer->Clear();
		else delete send_buffer;
		return false;
	}

	send_buffer->ex_over.session_key = GetSessionKey();
	++io_pending_count_;
	const int result = WSASend(socket_, &send_buffer->wsabuf, 1, 0, 0, &send_buffer->ex_over.over, 0);
	if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
		PostQueuedCompletionStatus(iocp_handle_, 0, SESSION_IO_COMPLETION, &send_buffer->ex_over.over);
		std::cerr << "Session::SendPacket() WSASend error\n";
		return false;
	}
	return true;
}


bool Session::RecvPacket()
{
	std::shared_lock<std::shared_mutex> socket_lock(socket_mutex_);
	if (life_state_.load() != LifeState::ACTIVE || socket_ == INVALID_SOCKET) return false;

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
	const LifeState life_state = life_state_.load();
	if (life_state == LifeState::NONE || life_state == LifeState::INITIALIZING) return false;
	std::lock_guard<std::mutex> lock(session_mutex_);
	return session_key_.session_index == session_key.session_index && session_key_.session_id == session_key.session_id;
}

void Session::AdjustRemainingDataSize(int data_size_delta)
{
	std::lock_guard<std::mutex> lock(session_mutex_);
	remaining_data_size_ += data_size_delta;
}

bool Session::BeginDisconnect(SessionKey session_key)
{
	std::unique_lock<std::shared_mutex> socket_lock(socket_mutex_);
	std::lock_guard<std::mutex> session_lock(session_mutex_);
	if (life_state_.load() != LifeState::ACTIVE) return false;
	if (session_key_.session_index != session_key.session_index || session_key_.session_id != session_key.session_id) return false;
	life_state_.store(LifeState::DISCONNECT_PENDING);
	const SOCKET socket = socket_;
	socket_ = INVALID_SOCKET;
	if (socket != INVALID_SOCKET) closesocket(socket);
	if (io_pending_count_.load() != 0) return false;
	LifeState expected_state = LifeState::DISCONNECT_PENDING;
	return life_state_.compare_exchange_strong(expected_state, LifeState::DISCONNECTING);
}

bool Session::CompleteIO(SessionKey session_key)
{
	if (!MatchesSessionKey(session_key) || io_pending_count_.load() <= 0) return false;
	const int pending_count = --io_pending_count_;
	if (pending_count != 0) return false;
	LifeState expected_state = LifeState::DISCONNECT_PENDING;
	return life_state_.compare_exchange_strong(expected_state, LifeState::DISCONNECTING);
}

bool Session::CompleteRoomRemoval(SessionKey session_key)
{
	std::lock_guard<std::mutex> session_lock(session_mutex_);
	const LifeState life_state = life_state_.load();
	if ((life_state != LifeState::DISCONNECT_PENDING && life_state != LifeState::DISCONNECTING) || mode_state_.load() != ModeState::ROOM) return false;
	if (session_key_.session_index != session_key.session_index || session_key_.session_id != session_key.session_id) return false;
	room_index_ = -1;
	mode_state_.store(ModeState::NONE);
	return life_state == LifeState::DISCONNECTING;
}

void Session::FinalizeDisconnect(SessionKey session_key)
{
	std::unique_lock<std::shared_mutex> socket_lock(socket_mutex_);
	if (life_state_.load() != LifeState::DISCONNECTING) return;
	std::lock_guard<std::mutex> session_lock(session_mutex_);
	if (session_key_.session_index != session_key.session_index || session_key_.session_id != session_key.session_id) return;
	room_index_ = -1;
	remaining_data_size_ = 0;
	session_key_.player_id = -1;
	session_key_.session_id = 0;
	ZeroMemory(&recv_over_.ex_over.over, sizeof(recv_over_.ex_over.over));
	ZeroMemory(recv_over_.packet_buffer, sizeof(recv_over_.packet_buffer));
	recv_over_.wsabuf.len = BUF_SIZE;
	recv_over_.wsabuf.buf = recv_over_.packet_buffer;
	recv_over_.ex_over.session_key = {};
	mode_state_.store(ModeState::NONE);
	life_state_.store(LifeState::NONE);
}

bool Session::TryEnqueueTask(SessionKey session_key, std::unique_ptr<SessionTask> task)
{
	if (!task) return false;
	std::lock_guard<std::mutex> session_lock(session_mutex_);
	if (life_state_.load() != LifeState::ACTIVE) return false;
	if (session_key_.session_index != session_key.session_index || session_key_.session_id != session_key.session_id) return false;
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

bool Session::TryStartTaskProcessing()
{
	bool expected = false;
	return is_processing_tasks_.compare_exchange_strong(expected, true);
}

void Session::CompleteTaskProcessing()
{
	is_processing_tasks_.store(false);
}

SessionKey Session::GetSessionKey() const
{
	std::lock_guard<std::mutex> lock(session_mutex_);
	return session_key_;
}

int Session::GetRoomIndex() const
{
	std::lock_guard<std::mutex> lock(session_mutex_);
	return room_index_;
}

RoomSnapshot Session::GetRoomSnapshot() const
{
	std::lock_guard<std::mutex> lock(session_mutex_);
	return { mode_state_.load(), room_index_ };
}

int Session::GetRemainingDataSize() const
{
	std::lock_guard<std::mutex> lock(session_mutex_);
	return remaining_data_size_;
}

bool Session::TrySetLobbyMode(SessionKey session_key)
{
	std::lock_guard<std::mutex> lock(session_mutex_);
	if (life_state_.load() != LifeState::ACTIVE || mode_state_.load() != ModeState::ROOM) return false;
	if (session_key_.session_index != session_key.session_index || session_key_.session_id != session_key.session_id) return false;
	room_index_ = -1;
	mode_state_.store(ModeState::LOBBY);
	return true;
}

bool Session::TrySetRoomMode(SessionKey session_key, int new_room_index)
{
	std::lock_guard<std::mutex> lock(session_mutex_);
	if (new_room_index < 0) return false;
	if (life_state_.load() != LifeState::ACTIVE) return false;
	if (mode_state_.load() != ModeState::LOBBY) return false;
	if (session_key_.session_index != session_key.session_index || session_key_.session_id != session_key.session_id) return false;
	room_index_ = new_room_index;
	mode_state_.store(ModeState::ROOM);
	return true;
}

bool Session::ApplyLoginResult(const DBLoginResult& result)
{
	std::lock_guard<std::mutex> lock(session_mutex_);
	if (life_state_.load() != LifeState::ACTIVE || mode_state_.load() != ModeState::LOGIN) return false;
	session_key_.player_id = result.player_id;
	nickname_ = result.nickname;
	mode_state_.store(ModeState::LOBBY);
	return true;
}

std::string Session::GetNickname() const
{
	std::lock_guard<std::mutex> lock(session_mutex_);
	return nickname_;
}
