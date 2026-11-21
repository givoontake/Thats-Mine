#pragma once
#include <WinSock2.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include "game_packets.h"
#include "movement_settings.h"
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

// 그 시각의 걷기 상태. 이동 상태와 그때 누르고 있는 이동 키(MoveKey 자리의 비트)다.
// 스폰 자리는 땅에서 SPAWN_HEIGHT_OFFSET 위라 스폰을 받은 뒤 땅까지 떨어지고, 걷기 전에는 그 자리에 서 있으며 누른 키가 없다.
struct WalkStep
{
	MOVE_STATE move_state;
	std::uint8_t move_keys;
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
	// 내 캐릭터 스폰을 받은 시각. 전송 프레임 0번이 시작한 시각이고, 받기 처리가 쓰고 보내기 스레드가 읽는다. 받기 전에는 -1이다.
	std::atomic<long long> frame_start_ms_{ -1 };
	// 보내기 스레드만 쓴다. 다음에 이동 패킷을 보낼 번호와 권총으로 쏜 횟수
	std::uint32_t next_move_frame_number_ = 0;
	std::uint32_t shot_count_ = 0;
	// 받은 가장 최근 방 틱 번호. 발사의 되감을 시점으로 보낸다.
	std::atomic<std::uint32_t> latest_tick_number_{ 0 };
	// 보내기 스레드가 보낸 이동을 넣고, 받기 처리가 내 이동 중계의 위치로 찾아 꺼낸다. 서버는 받은 위치를 그대로 중계한다.
	std::array<SentMove, SENT_MOVE_QUEUE_SIZE> sent_moves_{};
	int sent_move_head_ = 0;
	int sent_move_count_ = 0;
	std::mutex sent_move_mutex_;
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
	WalkStep GetWalkStep(long long time_ms) const;
	void AddSentMove(const float location[3], long long send_time_ms);
	// 같은 위치의 보낸 이동을 찾아 보낸 시각을 돌려준다. 그보다 먼저 보낸 이동은 중계되지 않은 것이라 함께 버린다.
	std::optional<long long> TakeSentMoveTime(const float location[3]);
	// 내 캐릭터 스폰을 받은 now_ms에 전송 프레임 0번을 시작한다.
	void StartFrameClock(long long now_ms) { frame_start_ms_.store(now_ms); }
	bool HasFrameClock() const { return frame_start_ms_.load() >= 0; }
	// now_ms가 속한 전송 프레임 번호와, 번호 frame_number가 끝나는 시각
	std::uint32_t GetFrameNumber(long long now_ms) const { return static_cast<std::uint32_t>((now_ms - frame_start_ms_.load()) / MOVE_STEP_MS); }
	long long GetFrameEndMs(std::uint32_t frame_number) const { return frame_start_ms_.load() + (static_cast<long long>(frame_number) + 1) * MOVE_STEP_MS; }
	std::uint32_t GetNextMoveFrameNumber() const { return next_move_frame_number_; }
	void SetNextMoveFrameNumber(std::uint32_t frame_number) { next_move_frame_number_ = frame_number; }
	std::uint32_t AddShotCount() { return ++shot_count_; }
	void MarkTickNumber(std::uint32_t tick_number) { if (static_cast<std::int32_t>(tick_number - latest_tick_number_.load()) > 0) latest_tick_number_.store(tick_number); }
	std::uint32_t GetLatestTickNumber() const { return latest_tick_number_.load(); }
	long long GetNextFireMs() const { return next_fire_ms_; }
	void SetNextFireMs(long long time_ms) { next_fire_ms_ = time_ms; }
	void SetFireSendMs(long long time_ms) { fire_send_ms_.store(time_ms); }
	long long TakeFireSendMs() { return fire_send_ms_.exchange(-1); }
	bool IsWalking(long long now_ms) const { const long long start_ms = GetWalkStartMs(); return start_ms >= 0 && now_ms >= start_ms; }
	// 걷기를 시작하는 시각. 스폰 자리에서 땅까지 떨어진 뒤다. 아직 정하지 않았으면 -1이다.
	long long GetWalkStartMs() const;

	SOCKET GetSocket() const { return socket_; }
	WSAOVERLAPPED& GetRecvOverlapped() { return recv_over_; }
	char* GetRecvBuffer() { return recv_buffer_; }
	int GetIndex() const { return index_; }
	int GetPlayerID() const { return player_id_; }
	int GetRemainingDataSize() const { return remaining_data_size_; }
	SessionState GetState() const { return state_.load(); }
	long long GetLastRoomListMs() const { return last_room_list_ms_; }
	int IncrementReadyPlayerCount() { return ready_player_count_.fetch_add(1) + 1; }

	void SetPlayerID(int player_id) { player_id_ = player_id; }
	void SetRemainingDataSize(int size) { remaining_data_size_ = size; }
	void SetLastRoomListMs(long long time_ms) { last_room_list_ms_ = time_ms; }
	bool TrySetState(SessionState expected, SessionState desired) { return state_.compare_exchange_strong(expected, desired); }
	void SetState(SessionState desired) { state_.store(desired); }
};
