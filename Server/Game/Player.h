#pragma once
#include <atomic>
#include <string>
#include "types.h"
#include "settings.h"
#include "ExOverlapped.h"

class Session;

class Player
{
	SessionKey session_key_; // 세션 정리 후에도 방 슬롯을 식별하고, 재사용된 세션 인덱스의 오삭제를 막는다.
	std::atomic<ActiveEntryState> active_state_{ ActiveEntryState::EMPTY };
	char send_buffer_[BUF_SIZE];
	int send_data_size_ = 0;
	std::atomic<RoomPlayerState> room_player_state_;



public:
	Player();
	~Player();

	SessionKey GetSessionKey() const { return session_key_; }
	bool HasSession() const { return active_state_.load() != ActiveEntryState::EMPTY; }
	bool IsActive() const { return active_state_.load() == ActiveEntryState::ACTIVE; }
	ActiveEntryState GetActiveState() const { return active_state_.load(); }
	bool MatchesSessionKey(SessionKey session_key) const;
	RoomPlayerState GetRoomPlayerState() const { return room_player_state_.load(); }
	char* GetSendBuffer() { return send_buffer_; }
	int GetSendDataSize() const { return send_data_size_; }

	void SetRoomPlayerState(RoomPlayerState new_state) { return room_player_state_.store(new_state); }

	bool InitPlayer(Session& session, SessionKey session_key, int room_index);
	bool Activate(SessionKey session_key);
	bool TrySetPending(SessionKey session_key);
	void ClearPlayer();
	void ResetGameData();
	bool AddToSendBuffer(const char* data, int data_size);
	void ClearSendBuffer();
};
