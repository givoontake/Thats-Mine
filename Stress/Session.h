#pragma once
#include <WinSock2.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include "game_packets.h"
#include "stress_settings.h"

enum class SessionState
{
	NONE,
	CONNECTING,
	LOGIN,
	LOBBY,
	ENTER_ROOM,
	ROOM,
	LOADING,
	PLAYING,
	DISCONNECTING
};

// 지금 시각의 걷기 상태. 이동 상태와 그때 누르고 있는 앞뒤 키 입력이다.
struct WalkStep
{
	MOVE_STATE move_state;
	std::int8_t input_forward;
};

// 보낸 이동 상태의 위치와 보낸 시각
struct SentMove
{
	float location[3];
	long long send_time_ms;
};

// 봇 하나의 연결. 받기는 겹친 입출력 하나를 계속 다시 걸고, 보내기는 패킷마다 겹친 입출력을 새로 만든다.
// 걷기는 스폰 자리에서 이웃 스폰 자리까지 앞으로 걷고 같은 길을 뒤로 걸어 돌아오기를 반복한다. 높이는 두 자리의 땅 높이 사이를 잇는다.
class Session
{
	SOCKET socket_ = INVALID_SOCKET;
	WSAOVERLAPPED recv_over_{};
	char recv_buffer_[RECV_BUFFER_SIZE]{};
	int index_ = -1;
	int player_id_ = -1;
	int remaining_data_size_ = 0;
	std::atomic<SessionState> state_{ SessionState::NONE };
	std::atomic<int> ready_player_count_{ 0 };
	long long last_room_list_ms_ = 0;
	// 걷기 시작 시각이 정해진 뒤에만 보내기 스레드가 걷기 값을 읽는다.
	float walk_start_[3]{};
	float walk_end_[3]{};
	float walk_yaw_ = 0.0f;
	std::atomic<long long> walk_start_ms_{ -1 };
	// 보내기 스레드만 쓴다.
	long long last_move_send_ms_ = 0;
	std::int8_t sent_input_forward_ = 0;
	bool has_sent_input_ = false;
	// 보내기 스레드가 보낸 이동을 넣고, 받기 처리가 내 이동 중계의 위치로 찾아 꺼낸다. 서버는 받은 위치를 그대로 중계한다.
	std::array<SentMove, SENT_MOVE_QUEUE_SIZE> sent_moves_{};
	int sent_move_head_ = 0;
	int sent_move_count_ = 0;
	std::mutex sent_move_mutex_;
	// 받기 처리가 서버 시각 동기화를 받아 쓰고, 보내기 스레드가 동작 시각을 만들 때 읽는다.
	std::atomic<long long> server_time_offset_ms_{ 0 };
	std::atomic<bool> has_time_sync_{ false };
	// 보내기 스레드만 쓴다. 장비를 바꾸기 전에는 -1이다.
	long long next_fire_ms_ = -1;
	// 보내기 스레드가 발사를 보낸 시각을 쓰고, 받기 처리가 탄약 사용 알림을 받으면 꺼낸다. 보낸 발사가 없으면 -1이다.
	std::atomic<long long> fire_send_ms_{ -1 };

public:
	bool SendPacket(const void* packet, int size);
	void RecvPacket(HANDLE iocp_handle);
	void InitSession(int index, SOCKET socket);
	void ClearSession();
	// 스폰 자리로 걷기 길을 정한다. 이웃 스폰 자리는 같은 x에서 y가 WALK_DISTANCE 떨어진 자리다.
	void SetWalkPath(float x, float y);
	void StartWalk(long long start_ms) { walk_start_ms_.store(start_ms); }
	WalkStep GetWalkStep(long long now_ms) const;
	void AddSentMove(const float location[3], long long send_time_ms);
	// 같은 위치의 보낸 이동을 찾아 보낸 시각을 돌려준다. 그보다 먼저 보낸 이동은 중계되지 않은 것이라 함께 버린다.
	std::optional<long long> TakeSentMoveTime(const float location[3]);
	void SetTimeSync(std::uint32_t server_time_ms, long long now_ms) { server_time_offset_ms_.store(static_cast<long long>(server_time_ms) - now_ms); has_time_sync_.store(true); }
	bool HasTimeSync() const { return has_time_sync_.load(); }
	// 받은 서버 시각에 받은 뒤 흐른 시간을 더한 동작 시각
	std::uint32_t GetActionTimeMs(long long now_ms) const { return static_cast<std::uint32_t>(now_ms + server_time_offset_ms_.load()); }
	long long GetNextFireMs() const { return next_fire_ms_; }
	void SetNextFireMs(long long time_ms) { next_fire_ms_ = time_ms; }
	void SetFireSendMs(long long time_ms) { fire_send_ms_.store(time_ms); }
	long long TakeFireSendMs() { return fire_send_ms_.exchange(-1); }
	bool IsWalking(long long now_ms) const { const long long start_ms = walk_start_ms_.load(); return start_ms >= 0 && now_ms >= start_ms; }

	SOCKET GetSocket() const { return socket_; }
	WSAOVERLAPPED& GetRecvOverlapped() { return recv_over_; }
	char* GetRecvBuffer() { return recv_buffer_; }
	int GetIndex() const { return index_; }
	int GetPlayerID() const { return player_id_; }
	int GetRemainingDataSize() const { return remaining_data_size_; }
	SessionState GetState() const { return state_.load(); }
	long long GetLastRoomListMs() const { return last_room_list_ms_; }
	long long GetLastMoveSendMs() const { return last_move_send_ms_; }
	std::int8_t GetSentInputForward() const { return sent_input_forward_; }
	bool HasSentInput() const { return has_sent_input_; }
	int IncrementReadyPlayerCount() { return ready_player_count_.fetch_add(1) + 1; }

	void SetPlayerID(int player_id) { player_id_ = player_id; }
	void SetRemainingDataSize(int size) { remaining_data_size_ = size; }
	void SetLastRoomListMs(long long time_ms) { last_room_list_ms_ = time_ms; }
	void SetLastMoveSendMs(long long time_ms) { last_move_send_ms_ = time_ms; }
	void SetSentInputForward(std::int8_t input_forward) { sent_input_forward_ = input_forward; has_sent_input_ = true; }
	bool TrySetState(SessionState expected, SessionState desired) { return state_.compare_exchange_strong(expected, desired); }
	void SetState(SessionState desired) { state_.store(desired); }
};
