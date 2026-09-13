#include "Player.h"
#include "Session.h"

Player::Player()
{
	room_player_state_.store(RoomPlayerState::WAIT);
}

Player::~Player()
{

}

bool Player::InitPlayer(Session& session, SessionKey session_key, int room_index)
{
	if (session.GetLifeState() != LifeState::ACTIVE) return false;
	if (!session.MatchesSessionKey(session_key)) return false;
	ActiveEntryState expected_state = ActiveEntryState::EMPTY;
	if (!active_state_.compare_exchange_strong(expected_state, ActiveEntryState::PENDING)) return false;
	session_key_ = session_key;
	const RoomSnapshot room_snapshot = session.GetRoomSnapshot();
	if (room_snapshot.mode_state == ModeState::LOBBY) {
		if (!session.TrySetRoomMode(session_key, room_index)) {
			ClearPlayer();
			return false;
		}
	}
	else if (room_snapshot.mode_state != ModeState::ROOM || room_snapshot.room_index != room_index) {
		ClearPlayer();
		return false;
	}
	room_player_state_.store(RoomPlayerState::WAIT);

	ClearSendBuffer();
	return true;
}

bool Player::Activate(SessionKey session_key)
{
	if (!MatchesSessionKey(session_key)) return false;
	ActiveEntryState expected_state = ActiveEntryState::PENDING;
	return active_state_.compare_exchange_strong(expected_state, ActiveEntryState::ACTIVE);
}

bool Player::TrySetPending(SessionKey session_key)
{
	if (!MatchesSessionKey(session_key)) return false;
	ActiveEntryState expected_state = ActiveEntryState::ACTIVE;
	return active_state_.compare_exchange_strong(expected_state, ActiveEntryState::PENDING);
}

bool Player::MatchesSessionKey(SessionKey session_key) const
{
	return HasSession() &&
		session_key_.session_index == session_key.session_index &&
		session_key_.player_id == session_key.player_id &&
		session_key_.session_id == session_key.session_id;
}

void Player::ClearPlayer()
{
	session_key_ = {};
	room_player_state_.store(RoomPlayerState::WAIT);

	ClearSendBuffer();
	active_state_.store(ActiveEntryState::EMPTY);
}

void Player::ResetGameData()
{
	room_player_state_.store(RoomPlayerState::WAIT);

	ClearSendBuffer();
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
	ZeroMemory(send_buffer_, sizeof(send_buffer_));
}
