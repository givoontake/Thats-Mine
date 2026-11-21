#include "Session.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include "ex_overlapped.h"
#include "movement_settings.h"
#include "server_settings.h"
#include "spawn_points.h"

bool Session::SendPacket(const void* packet, int size)
{
	const SessionState state = state_.load();
	if (state == SessionState::NONE || state == SessionState::DISCONNECTING || size <= 0 || size > BUF_SIZE) return false;
	ExOverlapped* send_over = new ExOverlapped;
	memcpy(send_over->packet_buffer, packet, size);
	send_over->wsabuf.buf = send_over->packet_buffer;
	send_over->wsabuf.len = static_cast<ULONG>(size);
	if (WSASend(socket_, &send_over->wsabuf, 1, nullptr, 0, &send_over->over, nullptr) == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
		delete send_over;
		return false;
	}
	return true;
}

void Session::RecvPacket(HANDLE iocp_handle)
{
	const SessionState state = state_.load();
	if (state == SessionState::NONE || state == SessionState::DISCONNECTING) return;
	ZeroMemory(&recv_over_, sizeof(recv_over_));
	WSABUF wsabuf{ static_cast<ULONG>(RECV_BUFFER_SIZE - remaining_data_size_), recv_buffer_ + remaining_data_size_ };
	DWORD flags = 0;
	if (WSARecv(socket_, &wsabuf, 1, nullptr, &flags, &recv_over_, nullptr) == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
		PostQueuedCompletionStatus(iocp_handle, 0, index_, &recv_over_);
	}
}

void Session::InitSession(int index, SOCKET socket)
{
	index_ = index;
	socket_ = socket;
	ClearSession();
}

void Session::ClearSession()
{
	player_id_ = -1;
	remaining_data_size_ = 0;
	ready_player_count_.store(0);
	last_room_list_ms_ = 0;
	walk_start_ms_.store(-1);
	frame_start_ms_.store(-1);
	next_move_frame_number_ = 0;
	shot_count_ = 0;
	latest_tick_number_.store(0);
	next_fire_ms_ = -1;
	fire_send_ms_.store(-1);
	std::lock_guard<std::mutex> lock(sent_move_mutex_);
	sent_move_head_ = 0;
	sent_move_count_ = 0;
}

void Session::SetWalkPath(float x, float y)
{
	// 받은 스폰 자리와 같은 자리를 찾고, 이웃은 y가 큰 쪽을 먼저 고른다.
	const SpawnPoint* start = nullptr;
	const SpawnPoint* end = nullptr;
	for (const SpawnPoint& point : START_SPAWN_POINTS) {
		if (point.x != x) continue;
		if (point.y == y) start = &point;
		else if (point.y == y + WALK_DISTANCE) end = &point;
		else if (!end && point.y == y - WALK_DISTANCE) end = &point;
	}
	const SpawnPoint& from = start ? *start : START_SPAWN_POINTS[0];
	const SpawnPoint& to = end ? *end : START_SPAWN_POINTS[2];
	walk_start_[0] = from.x;
	walk_start_[1] = from.y;
	walk_start_[2] = from.z - SPAWN_HEIGHT_OFFSET;
	walk_end_[0] = to.x;
	walk_end_[1] = to.y;
	walk_end_[2] = to.z - SPAWN_HEIGHT_OFFSET;
	walk_yaw_ = to.y > from.y ? 90.0f : -90.0f;
}

// 스폰 자리에서 땅까지 떨어지는 시간(ms)
static float GetSpawnFallMs()
{
	return std::sqrt(2.0f * SPAWN_HEIGHT_OFFSET / GRAVITY) * 1000.0f;
}

long long Session::GetWalkStartMs() const
{
	const long long start_ms = walk_start_ms_.load();
	if (start_ms < 0) return -1;
	return std::max(start_ms, frame_start_ms_.load() + static_cast<long long>(std::ceil(GetSpawnFallMs())));
}

WalkStep Session::GetWalkStep(long long time_ms) const
{
	WalkStep step{};
	step.move_state.yaw = walk_yaw_;
	step.move_state.view_pitch = 0.0f;
	step.move_state.is_falling = false;
	step.move_keys = 0;
	std::memcpy(step.move_state.location, walk_start_, sizeof(step.move_state.location));
	// 서버도 스폰 상태에서 전송 프레임마다 같은 중력으로 떨어뜨린다.
	const float fall_seconds = static_cast<float>(time_ms - frame_start_ms_.load()) / 1000.0f;
	if (fall_seconds * 1000.0f < GetSpawnFallMs()) {
		step.move_state.location[2] = walk_start_[2] + SPAWN_HEIGHT_OFFSET - 0.5f * GRAVITY * fall_seconds * fall_seconds;
		step.move_state.velocity[2] = -GRAVITY * fall_seconds;
		step.move_state.is_falling = true;
		return step;
	}
	const long long start_ms = GetWalkStartMs();
	if (start_ms < 0 || time_ms < start_ms) return step;
	const long long forward_ms = static_cast<long long>(WALK_DISTANCE / MOVE_SPEED_FORWARD * 1000.0f);
	const long long backward_ms = static_cast<long long>(WALK_DISTANCE / MOVE_SPEED_BACKWARD * 1000.0f);
	const long long elapsed_ms = (time_ms - start_ms) % (forward_ms + backward_ms);
	const bool is_forward = elapsed_ms < forward_ms;
	const float ratio = is_forward ? static_cast<float>(elapsed_ms) / forward_ms : 1.0f - static_cast<float>(elapsed_ms - forward_ms) / backward_ms;
	const float speed = is_forward ? MOVE_SPEED_FORWARD : -MOVE_SPEED_BACKWARD;
	for (int axis = 0; axis < 3; ++axis) {
		step.move_state.location[axis] = walk_start_[axis] + (walk_end_[axis] - walk_start_[axis]) * ratio;
		step.move_state.velocity[axis] = (walk_end_[axis] - walk_start_[axis]) / WALK_DISTANCE * speed;
	}
	step.move_keys = static_cast<std::uint8_t>(1u << static_cast<int>(is_forward ? MoveKey::FORWARD : MoveKey::BACKWARD));
	return step;
}

void Session::AddSentMove(const float location[3], long long send_time_ms)
{
	std::lock_guard<std::mutex> lock(sent_move_mutex_);
	if (sent_move_count_ == SENT_MOVE_QUEUE_SIZE) {
		sent_move_head_ = (sent_move_head_ + 1) % SENT_MOVE_QUEUE_SIZE;
		--sent_move_count_;
	}
	SentMove& sent_move = sent_moves_[(sent_move_head_ + sent_move_count_) % SENT_MOVE_QUEUE_SIZE];
	memcpy(sent_move.location, location, sizeof(sent_move.location));
	sent_move.send_time_ms = send_time_ms;
	++sent_move_count_;
}

std::optional<long long> Session::TakeSentMoveTime(const float location[3])
{
	std::lock_guard<std::mutex> lock(sent_move_mutex_);
	for (int i = 0; i < sent_move_count_; ++i) {
		const SentMove& sent_move = sent_moves_[(sent_move_head_ + i) % SENT_MOVE_QUEUE_SIZE];
		if (memcmp(sent_move.location, location, sizeof(sent_move.location)) != 0) continue;
		sent_move_head_ = (sent_move_head_ + i + 1) % SENT_MOVE_QUEUE_SIZE;
		sent_move_count_ -= i + 1;
		return sent_move.send_time_ms;
	}
	return std::nullopt;
}
