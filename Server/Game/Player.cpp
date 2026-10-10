#include <algorithm>
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
	ClearStun();
	ammo_.clear();
	ResetPickaxe();
}

void Player::ResetGameData()
{
	room_player_state_.store(RoomPlayerState::WAIT);

	ClearSendBuffer();
	ClearMoveState();
	ClearStun();
	ammo_.clear();
	ResetPickaxe();
}

void Player::ResetAmmo(std::size_t weapon_count)
{
	ammo_.assign(weapon_count, WeaponAmmo{});
}

void Player::UpdateReload(WeaponAmmo& ammo, const WeaponInfo& info, long long time_ms)
{
	if (ammo.reload_end_ms == 0 || time_ms < ammo.reload_end_ms) return;
	ammo.reload_end_ms = 0;
	const int loaded = std::min(static_cast<int>(info.magazine_capacity) - ammo.rounds, ammo.spare_rounds);
	ammo.rounds += loaded;
	ammo.spare_rounds -= loaded;
}

bool Player::TryFire(std::uint8_t weapon, const WeaponInfo& info, long long time_ms)
{
	if (weapon >= ammo_.size()) return false;
	WeaponAmmo& ammo = ammo_[weapon];
	UpdateReload(ammo, info, time_ms);
	if (ammo.reload_end_ms != 0 || ammo.rounds <= 0 || time_ms < ammo.next_fire_ms) return false;
	--ammo.rounds;
	ammo.next_fire_ms = time_ms + static_cast<long long>(info.fire_interval * 1000.0f) - AMMO_LATENCY_ALLOWANCE_MS;
	ammo.has_unclaimed_shot = true;
	return true;
}

bool Player::TryStartReload(std::uint8_t weapon, const WeaponInfo& info, long long time_ms)
{
	if (weapon >= ammo_.size()) return false;
	WeaponAmmo& ammo = ammo_[weapon];
	UpdateReload(ammo, info, time_ms);
	if (ammo.reload_end_ms != 0 || ammo.spare_rounds <= 0 || ammo.rounds >= static_cast<int>(info.magazine_capacity)) return false;
	ammo.reload_end_ms = time_ms + static_cast<long long>(info.reload_duration * 1000.0f) - AMMO_LATENCY_ALLOWANCE_MS;
	return true;
}

void Player::AddSpareRounds(std::uint8_t weapon, int count, const WeaponInfo& info, long long time_ms)
{
	if (weapon >= ammo_.size()) return;
	UpdateReload(ammo_[weapon], info, time_ms);
	ammo_[weapon].spare_rounds += count;
}

void Player::ResetPickaxe()
{
	pickaxe_charges_ = PICKAXE_MAX_CHARGES;
	pickaxe_charge_start_ms_ = 0;
}

void Player::UpdatePickaxeCharges(long long time_ms)
{
	if (pickaxe_charge_start_ms_ == 0) return;
	const long long gained = (time_ms - pickaxe_charge_start_ms_) / PICKAXE_CHARGE_MS;
	if (gained <= 0) return;
	pickaxe_charges_ = static_cast<int>(std::min<long long>(PICKAXE_MAX_CHARGES, pickaxe_charges_ + gained));
	pickaxe_charge_start_ms_ = pickaxe_charges_ == PICKAXE_MAX_CHARGES ? 0 : pickaxe_charge_start_ms_ + gained * PICKAXE_CHARGE_MS;
}

bool Player::TryUsePickaxe(long long time_ms)
{
	UpdatePickaxeCharges(time_ms);
	if (pickaxe_charges_ <= 0) return false;
	// 클라이언트는 휘두른 순간부터 충전을 세므로, 늦게 받은 만큼 일찍 시작한 것으로 본다.
	if (pickaxe_charges_ == PICKAXE_MAX_CHARGES) pickaxe_charge_start_ms_ = time_ms - AMMO_LATENCY_ALLOWANCE_MS;
	--pickaxe_charges_;
	return true;
}

long long Player::GetPickaxeChargeRemainingMs(long long time_ms) const
{
	if (pickaxe_charge_start_ms_ == 0) return 0;
	return PICKAXE_CHARGE_MS - (time_ms - pickaxe_charge_start_ms_) % PICKAXE_CHARGE_MS;
}

bool Player::ClaimShot(std::uint8_t weapon)
{
	if (weapon >= ammo_.size() || !ammo_[weapon].has_unclaimed_shot) return false;
	ammo_[weapon].has_unclaimed_shot = false;
	return true;
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

void Player::ClearStun()
{
	stun_gauge_ = 0.0f;
	stun_gauge_time_ms_ = 0;
	stun_end_ms_ = 0;
}

float Player::GetStunGauge(long long time_ms) const
{
	if (IsStunned(time_ms)) return STUN_GAUGE_MAX;
	return std::max(0.0f, stun_gauge_ - STUN_GAUGE_DECAY_PER_SECOND * (time_ms - stun_gauge_time_ms_) / 1000.0f);
}

// 기절하면 끝나는 시각에 게이지가 0이 되도록 기록해 두어, 끝난 뒤부터 0에서 다시 계산한다.
bool Player::AddStunGauge(float amount, long long time_ms)
{
	if (IsStunned(time_ms)) return false;
	stun_gauge_ = GetStunGauge(time_ms) + amount;
	stun_gauge_time_ms_ = time_ms;
	if (stun_gauge_ < STUN_GAUGE_MAX) return false;
	stun_end_ms_ = time_ms + STUN_DURATION_MS;
	stun_gauge_ = 0.0f;
	stun_gauge_time_ms_ = stun_end_ms_;
	return true;
}

void Player::StopMovement()
{
	move_state_.velocity[0] = 0.0f;
	move_state_.velocity[1] = 0.0f;
	move_state_.input_right = 0;
	move_state_.input_forward = 0;
	move_state_.is_aiming = false;
	has_new_move_state_ = true;
	has_input_change_ = true;
}

std::uint32_t Player::StartMoveCorrection()
{
	StopMovement();
	is_awaiting_move_correction_ = true;
	return ++move_correction_id_;
}

void Player::CompleteMoveCorrection(std::uint32_t correction_id)
{
	if (correction_id == move_correction_id_) is_awaiting_move_correction_ = false;
}

void Player::ClearMoveState()
{
	move_state_ = {};
	move_time_ms_ = 0;
	last_move_relay_ms_ = 0;
	has_new_move_state_ = false;
	has_input_change_ = false;
	is_awaiting_move_correction_ = false;
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
