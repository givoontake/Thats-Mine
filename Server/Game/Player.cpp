#include <cstring>
#include "Player.h"

Player::Player()
{
	room_player_state_.store(RoomPlayerState::WAIT);
}

Player::~Player()
{

}

bool Player::InitPlayer(SessionKey session_key)
{
	if (HasSession() || session_key.session_id == 0) return false;
	session_key_ = session_key;
	room_player_state_.store(RoomPlayerState::WAIT);

	ClearSendBuffer();
	return true;
}

bool Player::MatchesSessionKey(SessionKey session_key) const
{
	return HasSession() && session_key_ == session_key;
}

void Player::ClearPlayer()
{
	session_key_ = {};
	room_player_state_.store(RoomPlayerState::WAIT);

	ClearSendBuffer();
	ClearMoveState();
}

void Player::ResetGameData()
{
	room_player_state_.store(RoomPlayerState::WAIT);

	ClearSendBuffer();
	ClearMoveState();
}

void Player::SetMoveState(const MOVE_STATE& move_state, long long time_ms, bool is_input_change)
{
	move_state_ = move_state;
	move_time_ms_ = time_ms;
	has_new_move_state_ = true;
	has_input_change_ = has_input_change_ || is_input_change;
}

void Player::MarkMoveStateRelayed(long long tick_time_ms)
{
	last_move_relay_ms_ = tick_time_ms;
	has_new_move_state_ = false;
	has_input_change_ = false;
}

void Player::ClearMoveState()
{
	move_state_ = {};
	move_time_ms_ = 0;
	last_move_relay_ms_ = 0;
	has_new_move_state_ = false;
	has_input_change_ = false;
}

bool Player::AddToSendBuffer(const char* data, int data_size)
{
	if (data_size <= 0) return true;
	const int need_size = send_data_size_ + data_size;
	if (need_size > BUF_SIZE) return false;
	memcpy(send_buffer_ + send_data_size_, data, data_size);
	send_data_size_ += data_size;
	return true;
}

void Player::ClearSendBuffer()
{
	send_data_size_ = 0;
	memset(send_buffer_, 0, sizeof(send_buffer_));
}
