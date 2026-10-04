#pragma once
#include <atomic>
#include <string>
#include "types.h"
#include "settings.h"
#include "enum_class.h"
#include "game_packets.h"
#include "CharacterPose.h"

// 방 슬롯 하나를 나타낸다. 방을 처리하는 스레드만 변경하므로 슬롯 사용 여부는 세션 키 유무로 판단한다.
class Player
{
	SessionKey session_key_; // 세션 정리 후에도 방 슬롯을 식별하고, 재사용된 세션 인덱스의 오삭제를 막는다.
	char send_buffer_[BUF_SIZE];
	int send_data_size_ = 0;
	std::atomic<RoomPlayerState> room_player_state_;

	// 중계 간격 안에 여러 번 받으면 최신 이동 상태만 남긴다. 입력 변화가 섞여 있으면 간격을 기다리지 않고 다음 틱에 중계한다.
	MOVE_STATE move_state_{};
	long long move_time_ms_ = 0;
	long long last_move_relay_ms_ = 0;
	bool has_new_move_state_ = false;
	bool has_input_change_ = false;
	// 명중 검증에 쓰는 서버 판정용 자세. 이동 상태와 동작으로 애니메이션 진행을 따라간다.
	CharacterPose pose_;

	void ClearMoveState();



public:
	Player();
	~Player();

	SessionKey GetSessionKey() const { return session_key_; }
	bool HasSession() const { return session_key_.session_id != 0; }
	bool MatchesSessionKey(SessionKey session_key) const;
	RoomPlayerState GetRoomPlayerState() const { return room_player_state_.load(); }
	char* GetSendBuffer() { return send_buffer_; }
	int GetSendDataSize() const { return send_data_size_; }
	const MOVE_STATE& GetMoveState() const { return move_state_; }
	long long GetMoveTimeMs() const { return move_time_ms_; }
	bool HasMoveState() const { return move_time_ms_ != 0; }
	CharacterPose& GetPose() { return pose_; }
	const CharacterPose& GetPose() const { return pose_; }
	bool CanRelayMoveState(long long tick_time_ms) const { return has_new_move_state_ && (has_input_change_ || tick_time_ms - last_move_relay_ms_ >= MOVE_RELAY_INTERVAL_MS); }

	void SetRoomPlayerState(RoomPlayerState new_state) { return room_player_state_.store(new_state); }

	bool InitPlayer(SessionKey session_key);
	void ClearPlayer();
	void ResetGameData();
	bool AddToSendBuffer(const char* data, int data_size);
	void ClearSendBuffer();
	void SetMoveState(const MOVE_STATE& move_state, long long time_ms, bool is_input_change);
	void MarkMoveStateRelayed(long long tick_time_ms);
};
