#include <algorithm>
#include <cmath>
#include <cstring>
#include <random>
#include "Player.h"
#include "random_seed.h"
#include "shot_spread.h"

// 이동 상태의 최대 수평 속도(cm/s). 공중에서는 뛰어오를 때의 속도를 그대로 유지하므로 상태와 관계없이 앞 속도로 본다.
static float GetMaxMoveSpeed(const MOVE_STATE& move_state)
{
	if (move_state.air_phase != AirPhase::GROUND) return MOVE_SPEED_FORWARD;
	if (move_state.is_aiming) return MOVE_SPEED_AIMING;
	return move_state.input_forward < 0 ? MOVE_SPEED_BACKWARD : MOVE_SPEED_FORWARD;
}

Player::Player()
{
	room_player_state_.store(RoomPlayerState::WAIT);
	ResetItems();
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
	ResetHints();
	ResetItems();
	ResetBlocks();
	award_counts_.fill(0);
	award_reach_ms_.fill(0);
}

void Player::ResetGameData()
{
	room_player_state_.store(RoomPlayerState::WAIT);

	ClearSendBuffer();
	ClearMoveState();
	ClearStun();
	ammo_.clear();
	ResetPickaxe();
	ResetHints();
	ResetItems();
	ResetBlocks();
	award_counts_.fill(0);
	award_reach_ms_.fill(0);
}

void Player::AddAwardCount(AwardCategory category, long long time_ms)
{
	if (category >= AwardCategory::COUNT) return;
	++award_counts_[static_cast<std::size_t>(category)];
	award_reach_ms_[static_cast<std::size_t>(category)] = time_ms;
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
	ammo.has_untraced_shot = true;
	// 서버가 받은 발사 간격과 상태 변화는 지연 때문에 실제와 다를 수 있어, 최소치는 연사 열기를 더 회복하고 막 바뀐 상태는 유리한 쪽으로 본다.
	// 조준한 채 움직이는 퍼짐은 요구하지 않는다.
	const ShotSpreadSettings& spread = GetShotSpreadSettings(weapon);
	const float elapsed_seconds = (time_ms - ammo.spread_time_ms) / 1000.0f;
	const bool has_state = HasMoveState();
	const bool is_falling = has_state && move_state_.air_phase != AirPhase::GROUND && time_ms - fall_change_ms_ >= SHOT_SPREAD_STATE_ALLOWANCE_MS;
	const bool is_unaimed = !has_state || (!move_state_.is_aiming && time_ms - aim_change_ms_ >= SHOT_SPREAD_STATE_ALLOWANCE_MS);
	ammo.last_shot_min_spread = GetShotSpread(spread, RecoverShotHeat(ammo.spread_heat, elapsed_seconds + SHOT_SPREAD_TIME_ALLOWANCE_SECONDS, info.fire_interval), !is_unaimed, 0.0f, is_falling);
	ammo.last_shot_index = ammo.shot_count++;
	ammo.spread_heat = AddShotHeat(spread, RecoverShotHeat(ammo.spread_heat, elapsed_seconds, info.fire_interval));
	ammo.spread_time_ms = time_ms;
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

void Player::AddHint(std::uint32_t hint)
{
	if (std::find(hint_indices_.begin(), hint_indices_.end(), hint) != hint_indices_.end()) return;
	hint_indices_.push_back(hint);
}

std::optional<std::uint32_t> Player::TakeRandomHint()
{
	if (hint_indices_.empty()) return std::nullopt;
	std::mt19937 random(CreateRandomSeed());
	const std::size_t index = std::uniform_int_distribution<std::size_t>(0, hint_indices_.size() - 1)(random);
	const std::uint32_t hint = hint_indices_[index];
	hint_indices_.erase(hint_indices_.begin() + index);
	return hint;
}

void Player::ResetPickaxe()
{
	pickaxe_charges_ = PICKAXE_MAX_CHARGES;
	pickaxe_charge_start_ms_ = 0;
}

void Player::UpdatePickaxeCharges(long long time_ms)
{
	if (pickaxe_charge_start_ms_ == 0) return;
	const int charge_ms = GetPickaxeChargeMs();
	const long long gained = (time_ms - pickaxe_charge_start_ms_) / charge_ms;
	if (gained <= 0) return;
	pickaxe_charges_ = static_cast<int>(std::min<long long>(PICKAXE_MAX_CHARGES, pickaxe_charges_ + gained));
	pickaxe_charge_start_ms_ = pickaxe_charges_ == PICKAXE_MAX_CHARGES ? 0 : pickaxe_charge_start_ms_ + gained * charge_ms;
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
	const int charge_ms = GetPickaxeChargeMs();
	return charge_ms - (time_ms - pickaxe_charge_start_ms_) % charge_ms;
}

int Player::GetPickaxeChargeMs() const
{
	const std::uint8_t grade = item_grades_[static_cast<std::size_t>(ItemKind::PICKAXE_CHARGE)];
	return grade == NO_ITEM_GRADE ? PICKAXE_CHARGE_MS : PICKAXE_CHARGE_MS - ITEM_PICKAXE_CHARGE_REDUCTIONS_MS[grade];
}

float Player::GetStunGaugeDecayPerSecond() const
{
	const std::uint8_t grade = item_grades_[static_cast<std::size_t>(ItemKind::STUN_GAUGE_DECAY)];
	return grade == NO_ITEM_GRADE ? STUN_GAUGE_DECAY_PER_SECOND : ITEM_STUN_GAUGE_DECAYS_PER_SECOND[grade];
}

int Player::GetStunGaugeDecayDelayMs() const
{
	const std::uint8_t grade = item_grades_[static_cast<std::size_t>(ItemKind::STUN_GAUGE_DECAY_DELAY)];
	return grade == NO_ITEM_GRADE ? STUN_GAUGE_DECAY_DELAY_MS : ITEM_STUN_GAUGE_DECAY_DELAYS_MS[grade];
}

int Player::GetStunProtectionMs() const
{
	const std::uint8_t grade = item_grades_[static_cast<std::size_t>(ItemKind::STUN_PROTECTION)];
	return grade == NO_ITEM_GRADE ? STUN_PROTECTION_MS : ITEM_STUN_PROTECTIONS_MS[grade];
}

void Player::AddItem(ItemKind kind, std::uint8_t grade)
{
	if (grade >= std::size(ITEM_BOX_COUNTS) || kind >= ItemKind::COUNT) return;
	// 등급 값은 높은 등급일수록 작다.
	std::uint8_t& current = item_grades_[static_cast<std::size_t>(kind)];
	if (current == NO_ITEM_GRADE || grade < current) current = grade;
}

void Player::ResetItems()
{
	item_grades_.fill(NO_ITEM_GRADE);
}

void Player::UseBlock(std::uint8_t piece, std::uint64_t excluded_cells)
{
	--block_counts_[piece];
	kept_excluded_cells_ |= excluded_cells;
}

void Player::ResetBlocks()
{
	block_counts_.fill(0);
	kept_excluded_cells_ = 0;
	sent_exclusion_board_ = 0;
}

bool Player::ClaimShot(std::uint8_t weapon)
{
	if (weapon >= ammo_.size() || !ammo_[weapon].has_unclaimed_shot) return false;
	ammo_[weapon].has_unclaimed_shot = false;
	return true;
}

bool Player::ClaimShotTrace(std::uint8_t weapon)
{
	if (weapon >= ammo_.size() || !ammo_[weapon].has_untraced_shot) return false;
	ammo_[weapon].has_untraced_shot = false;
	return true;
}

void Player::SetMoveState(const MOVE_STATE& move_state, long long time_ms, bool is_input_change)
{
	if (move_state.is_aiming != move_state_.is_aiming) aim_change_ms_ = time_ms;
	if ((move_state.air_phase == AirPhase::GROUND) != (move_state_.air_phase == AirPhase::GROUND)) fall_change_ms_ = time_ms;
	move_excess_ = GetMoveExcess(move_state, time_ms);
	air_state_ = GetNextAirState(move_state, time_ms);
	move_state_ = move_state;
	move_time_ms_ = time_ms;
	has_new_move_state_ = true;
	has_input_change_ = has_input_change_ || is_input_change;
}

void Player::SetSpawnLocation(const float location[3], float yaw, long long time_ms)
{
	ClearMoveState();
	std::memcpy(move_state_.location, location, sizeof(move_state_.location));
	move_state_.yaw = yaw;
	move_time_ms_ = time_ms;
}

float Player::GetMoveExcess(const MOVE_STATE& move_state, long long time_ms) const
{
	// 두 위치 사이에 상태가 바뀌었을 수 있어 앞뒤 상태 중 빠른 쪽 속도를 쓴다.
	const float max_speed = std::max(GetMaxMoveSpeed(move_state_), GetMaxMoveSpeed(move_state));
	const float distance = std::hypot(move_state.location[0] - move_state_.location[0], move_state.location[1] - move_state_.location[1]);
	// 지난 시간만큼 줄이되 0 아래로는 내리지 않고 거리를 더한다. 오래 보내지 않다가 한 번에 멀리 가도 그 거리가 허용치를 넘으면 걸린다.
	return std::max(0.0f, move_excess_ - max_speed * static_cast<float>(time_ms - move_time_ms_) / 1000.0f) + distance;
}

AirState Player::GetAirState(long long time_ms) const
{
	if (air_state_.phase == AirPhase::RISING && GetAirElapsedSeconds(air_state_, time_ms) > JUMP_APEX_SECONDS + MOVE_AIR_DELAY_SECONDS) {
		return { AirPhase::FALLING, air_state_.start_z + JUMP_APEX_HEIGHT, air_state_.start_ms + static_cast<long long>(JUMP_APEX_SECONDS * 1000.0f) };
	}
	return air_state_;
}

AirState Player::GetNextAirState(const MOVE_STATE& move_state, long long time_ms) const
{
	const AirState air = GetAirState(time_ms);
	if (move_state.air_phase == AirPhase::GROUND) return {};
	if (air.phase == AirPhase::GROUND) return { move_state.air_phase, move_state_.location[2], move_time_ms_ };
	if (air.phase == AirPhase::RISING && move_state.air_phase == AirPhase::FALLING) {
		const long long apex_ms = air.start_ms + static_cast<long long>(JUMP_APEX_SECONDS * 1000.0f);
		return { AirPhase::FALLING, std::min(move_state.location[2], air.start_z + JUMP_APEX_HEIGHT), std::min(time_ms, apex_ms) };
	}
	return air;
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
	const long long decay_ms = std::max(0LL, time_ms - stun_gauge_time_ms_ - GetStunGaugeDecayDelayMs());
	return std::max(0.0f, stun_gauge_ - GetStunGaugeDecayPerSecond() * decay_ms / 1000.0f);
}

// 기절하면 끝나는 시각에 게이지가 0이 되도록 기록해 두어, 끝난 뒤부터 0에서 다시 계산한다.
bool Player::AddStunGauge(float amount, long long time_ms)
{
	if (IsStunProtected(time_ms)) return false;
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

std::uint32_t Player::StartMoveCorrection(const float location[3], float velocity_z, const AirState& air, long long time_ms)
{
	StopMovement();
	std::memcpy(move_state_.location, location, sizeof(move_state_.location));
	move_state_.velocity[2] = velocity_z;
	move_state_.air_phase = air.phase;
	move_time_ms_ = time_ms;
	air_state_ = air;
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
	move_excess_ = 0.0f;
	air_state_ = {};
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
