#include <algorithm>
#include <cmath>
#include <cstring>
#include <random>
#include "Player.h"
#include "random_seed.h"
#include "shot_spread.h"

// 땅에서 이동 키 입력과 조준 여부로 낼 수 있는 최대 수평 속도(cm/s)
static float GetMaxMoveSpeed(std::int8_t input_forward, bool is_aiming)
{
	if (is_aiming) return MOVE_SPEED_AIMING;
	return input_forward < 0 ? MOVE_SPEED_BACKWARD : MOVE_SPEED_FORWARD;
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
	const bool is_unaimed = !has_state || (!is_aiming_ && time_ms - aim_change_ms_ >= SHOT_SPREAD_STATE_ALLOWANCE_MS);
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

bool Player::AddHint(std::uint32_t hint)
{
	if (std::find(hint_indices_.begin(), hint_indices_.end(), hint) != hint_indices_.end()) return false;
	hint_indices_.push_back(hint);
	return true;
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

void Player::SetMoveState(const MOVE_STATE& move_state, long long time_ms, bool is_on_steep_slope, bool is_open_air)
{
	if ((move_state.air_phase == AirPhase::GROUND) != (move_state_.air_phase == AirPhase::GROUND)) fall_change_ms_ = time_ms;
	move_excess_ = GetMoveExcess(move_state, time_ms);
	speed_bound_ = GetMoveReach(move_state, time_ms).second;
	// 직전과 이번 위치가 모두 비탈 접촉이어야 비탈을 타고 온 구간으로 본다.
	air_state_ = GetNextAirState(move_state, time_ms, is_on_steep_slope && was_on_steep_slope_);
	was_on_steep_slope_ = is_on_steep_slope;
	clock_drift_ms_ = GetClockDrift(move_state, time_ms).value_or(0);
	has_client_time_ = true;
	// 허공 낙하 검사의 속도를 구할 구간이 너무 짧으면 시각 단위 오차가 커지므로, 이번 상태가 직전 상태와 가까우면 더 앞의 기준점을 그대로 둔다.
	if (!is_open_air || static_cast<std::int32_t>(move_state.client_time_ms - move_state_.client_time_ms) >= AIR_STEP_MIN_BASELINE_MS) {
		previous_move_z_ = move_state_.location[2];
		previous_client_time_ms_ = move_state_.client_time_ms;
	}
	open_air_count_ = is_open_air ? open_air_count_ + 1 : 0;
	has_urgent_move_state_ = has_urgent_move_state_ || move_state.air_phase != move_state_.air_phase;
	move_state_ = move_state;
	move_time_ms_ = time_ms;
	has_new_move_state_ = true;
	// 지금 입력은 다음 위치까지 이어지므로 상한을 지금 입력으로 다시 시작한다.
	move_speed_cap_ = GetMaxMoveSpeed(input_forward_, is_aiming_);
}

void Player::SetMoveInput(std::int8_t input_right, std::int8_t input_forward)
{
	input_right_ = input_right;
	input_forward_ = input_forward;
	move_speed_cap_ = std::max(move_speed_cap_, GetMaxMoveSpeed(input_forward_, is_aiming_));
}

void Player::SetAiming(bool is_aiming, long long time_ms)
{
	if (is_aiming != is_aiming_) aim_change_ms_ = time_ms;
	is_aiming_ = is_aiming;
	move_speed_cap_ = std::max(move_speed_cap_, GetMaxMoveSpeed(input_forward_, is_aiming_));
	pose_.SetAiming(is_aiming);
}

void Player::SetWeapon(std::uint8_t weapon, long long time_ms)
{
	weapon_ = weapon;
	pose_.ChangeWeapon(weapon, time_ms);
}

void Player::SetSpawnLocation(const float location[3], float yaw, long long time_ms)
{
	ClearMoveState();
	std::memcpy(move_state_.location, location, sizeof(move_state_.location));
	move_state_.yaw = yaw;
	move_time_ms_ = time_ms;
}

std::optional<long long> Player::GetClockDrift(const MOVE_STATE& move_state, long long time_ms) const
{
	if (!has_client_time_) return 0;
	// 클라이언트 시각은 32비트로 돌아가므로 차이를 부호 있는 값으로 본다.
	const std::int32_t client_elapsed_ms = static_cast<std::int32_t>(move_state.client_time_ms - move_state_.client_time_ms);
	if (client_elapsed_ms < 0) return std::nullopt;
	return std::max(-CLOCK_MAX_BEHIND_MS, clock_drift_ms_ + client_elapsed_ms - (time_ms - move_time_ms_));
}

std::optional<std::pair<float, float>> Player::GetPredictedAirZ(const MOVE_STATE& move_state) const
{
	if (open_air_count_ < 2) return std::nullopt;
	const float seconds = static_cast<float>(move_state.client_time_ms - move_state_.client_time_ms) / 1000.0f;
	const float previous_seconds = static_cast<float>(move_state_.client_time_ms - previous_client_time_ms_) / 1000.0f;
	// 같은 프레임에 두 번 보낸 상태는 움직임이 없어 판정하지 않는다. 오래 끊긴 사이는 공중 시간 초과가 맡는다.
	// 속도를 구할 구간이 짧으면 시각 단위 오차가 커지므로 판정하지 않고, 기준점이 앞에 남아 있어 곧 긴 구간으로 다시 판정한다.
	if (seconds <= 0.0f || previous_seconds * 1000.0f < AIR_STEP_MIN_BASELINE_MS || seconds > MOVE_AIR_DELAY_SECONDS) return std::nullopt;
	// 두 상태 사이 평균 속도는 그 가운데 시각의 속도이므로, 끝 시각 속도는 중력으로 그 절반 시간만큼 더 빨라진 값이다.
	const float velocity = std::max(-TERMINAL_FALL_SPEED, (move_state_.location[2] - previous_move_z_) / previous_seconds - GRAVITY * previous_seconds / 2.0f);
	const float tolerance = AIR_STEP_TOLERANCE + std::abs(velocity) * AIR_STEP_TIME_UNIT_SECONDS * (1.0f + seconds / previous_seconds);
	return std::pair<float, float>{ move_state_.location[2] + GetFreeFallDisplacement(velocity, seconds), tolerance };
}

// speed(cm/s)에서 seconds초 동안 상한 cap을 향해 언리얼 걷기와 같이 가속하거나 제동할 때 갈 수 있는 수평 거리(cm)와 끝 속도.
// 단계마다 속도를 먼저 바꾸고 그 속도로 움직인다.
static std::pair<float, float> GetWalkReach(float speed, float cap, float seconds)
{
	float distance = 0.0f;
	for (float remaining = seconds; remaining > 0.0f; remaining -= MOVE_BRAKING_STEP_SECONDS) {
		const float step = std::min(remaining, MOVE_BRAKING_STEP_SECONDS);
		if (speed < cap) speed = std::min(cap, speed + MOVE_MAX_ACCELERATION * step);
		else speed = std::max(cap, speed - (MOVE_GROUND_FRICTION * MOVE_BRAKING_FRICTION_FACTOR * speed + MOVE_BRAKING_DECELERATION) * step);
		distance += speed * step;
	}
	return { distance, speed };
}

std::pair<float, float> Player::GetMoveReach(const MOVE_STATE& move_state, long long time_ms) const
{
	// 지난 시간은 클라이언트 시각 차이로 잰다. 네트워크가 막혔다 몰려 와도 간격이 정확하고, 서버 시각보다 앞서 흐르는 것은 시각 검사가 막는다. 기준이 없으면 서버가 받은 시각 차이를 쓴다.
	const float elapsed = has_client_time_ ? static_cast<float>(static_cast<std::int32_t>(move_state.client_time_ms - move_state_.client_time_ms)) / 1000.0f : static_cast<float>(time_ms - move_time_ms_) / 1000.0f;
	// 오래 보내지 않다가 한 번에 멀리 가는 것은 정해진 시간까지만 인정한다.
	const float seconds = std::clamp(elapsed, 0.0f, MOVE_MAX_STEP_SECONDS);
	if (move_state_.air_phase == AirPhase::GROUND && move_state.air_phase == AirPhase::GROUND) {
		// 두 위치 사이에 입력이 바뀌었을 수 있어 그 사이 입력 중 빠른 쪽 상한을 쓴다. 상한이 낮아지면 다음 구간부터 제동식으로 줄어든다.
		// 기절해 입력이 없으면 상한이 0이라 제동식으로 멈춘다.
		const float cap = IsInputLocked(time_ms) ? 0.0f : move_speed_cap_;
		return GetWalkReach(speed_bound_, cap, seconds);
	}
	// 공중에서는 키로 속도를 바꿀 수 없어 속도를 바꾸는 것이 중력과 면에 닿는 것뿐이므로, 구간 끝 속도는 시작 속도에서 이번 구간에 내려간 높이만큼만 빨라지고 올라간 만큼 느려진다.
	// 땅에서 떠나는 구간은 땅에서 낼 수 있는 수평 속도에 점프의 수직 속도를 더해 시작한다.
	float start_speed = speed_bound_;
	if (move_state_.air_phase == AirPhase::GROUND) {
		const float ground_speed = GetWalkReach(speed_bound_, IsInputLocked(time_ms) ? 0.0f : move_speed_cap_, seconds).second;
		start_speed = move_state.air_phase == AirPhase::RISING ? std::hypot(ground_speed, JUMP_Z_VELOCITY) : ground_speed;
	}
	const float end_speed = std::sqrt(std::max(0.0f, start_speed * start_speed + 2.0f * GRAVITY * (move_state_.location[2] - move_state.location[2])));
	return { std::max(start_speed, end_speed) * seconds, end_speed };
}

float Player::GetMoveStepExcess(const MOVE_STATE& move_state, long long time_ms) const
{
	// 앞뒤 중 하나라도 공중이면 속도 상한이 3차원 속도이므로 3차원 거리를 본다.
	const bool is_on_air = move_state_.air_phase != AirPhase::GROUND || move_state.air_phase != AirPhase::GROUND;
	const float dx = move_state.location[0] - move_state_.location[0];
	const float dy = move_state.location[1] - move_state_.location[1];
	const float distance = is_on_air ? std::hypot(dx, dy, move_state.location[2] - move_state_.location[2]) : std::hypot(dx, dy);
	return distance - GetMoveReach(move_state, time_ms).first;
}

float Player::GetMoveExcess(const MOVE_STATE& move_state, long long time_ms) const
{
	return std::max(0.0f, move_excess_ + GetMoveStepExcess(move_state, time_ms));
}

AirState Player::GetAirState(long long time_ms) const
{
	if (air_state_.phase == AirPhase::RISING && GetAirElapsedSeconds(air_state_, time_ms) > JUMP_APEX_SECONDS + MOVE_AIR_DELAY_SECONDS) {
		return { AirPhase::FALLING, air_state_.start_z + JUMP_APEX_HEIGHT, air_state_.start_ms + static_cast<long long>(JUMP_APEX_SECONDS * 1000.0f) };
	}
	return air_state_;
}

AirState Player::GetNextAirState(const MOVE_STATE& move_state, long long time_ms, bool is_slope_slide) const
{
	const AirState air = GetAirState(time_ms);
	if (move_state.air_phase == AirPhase::GROUND) return {};
	AirState next = air;
	if (air.phase == AirPhase::GROUND) {
		// 오르막·계단에서는 직전 위치보다 높은 곳에서 점프하므로 점프를 알린 위치에서 시작한다. 그 위치가 땅 위인지는 이동 검사가 본다.
		if (move_state.air_phase == AirPhase::RISING) return { AirPhase::RISING, move_state.location[2], time_ms };
		next = { move_state.air_phase, move_state_.location[2], move_time_ms_ };
	}
	else if (air.phase == AirPhase::RISING && move_state.air_phase == AirPhase::FALLING) {
		const long long apex_ms = air.start_ms + static_cast<long long>(JUMP_APEX_SECONDS * 1000.0f);
		next = { AirPhase::FALLING, std::min(move_state.location[2], air.start_z + JUMP_APEX_HEIGHT), std::min(time_ms, apex_ms) };
	}
	if (next.phase != AirPhase::FALLING || !is_slope_slide) return next;
	// 공중에서 수직 속도를 바꾸는 것은 중력과 면에 닿는 것뿐이므로, 비탈을 타고 떨어지는 동안은 비탈이 늦춘 몫을 남기지 않도록 이번 위치에서 곡선을 다시 잡는다.
	// 직전 위치부터의 하강 속도 v는 v/g초 전에 v²/2g 위에서 속도 0으로 떨어지기 시작한 것과 같다.
	const float seconds = static_cast<float>(time_ms - move_time_ms_) / 1000.0f;
	const float fall_speed = seconds > 0.01f ? std::clamp((move_state_.location[2] - move_state.location[2]) / seconds, 0.0f, TERMINAL_FALL_SPEED) : 0.0f;
	return { AirPhase::FALLING, move_state.location[2] + fall_speed * fall_speed / (2.0f * GRAVITY), time_ms - static_cast<long long>(fall_speed / GRAVITY * 1000.0f) };
}

void Player::MarkMoveStateRelayed(long long tick_time_ms)
{
	last_move_relay_ms_ = tick_time_ms;
	has_new_move_state_ = false;
	has_urgent_move_state_ = false;
}

void Player::ClearStun()
{
	stun_gauge_ = 0.0f;
	stun_gauge_time_ms_ = 0;
	stun_end_ms_ = 0;
	has_pending_stun_release_ = false;
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
	has_pending_stun_release_ = true;
	return true;
}

bool Player::TakeStunRelease(long long time_ms)
{
	if (!has_pending_stun_release_ || IsStunned(time_ms)) return false;
	has_pending_stun_release_ = false;
	return true;
}

void Player::StopMovement()
{
	move_state_.velocity[0] = 0.0f;
	move_state_.velocity[1] = 0.0f;
	has_new_move_state_ = true;
	has_urgent_move_state_ = true;
}

std::uint32_t Player::StartMoveCorrection(const float location[3], float velocity_z, const AirState& air, long long time_ms)
{
	StopMovement();
	std::memcpy(move_state_.location, location, sizeof(move_state_.location));
	move_state_.velocity[2] = velocity_z;
	move_state_.air_phase = air.phase;
	move_time_ms_ = time_ms;
	air_state_ = air;
	// 옮긴 뒤에는 직전 상태로 속도를 구할 수 없고, 서버 시각이 맞춘 시각으로 바뀌어 시계 기준도 다시 잡는다.
	open_air_count_ = 0;
	has_client_time_ = false;
	clock_drift_ms_ = 0;
	// 맞춘 자리에서 수평 속도 없이 다시 시작한다. 공중이면 곡선의 수직 속도만 남는다.
	speed_bound_ = std::abs(velocity_z);
	was_on_steep_slope_ = false;
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
	speed_bound_ = 0.0f;
	was_on_steep_slope_ = false;
	air_state_ = {};
	previous_move_z_ = 0.0f;
	previous_client_time_ms_ = 0;
	open_air_count_ = 0;
	clock_drift_ms_ = 0;
	has_client_time_ = false;
	last_move_relay_ms_ = 0;
	has_new_move_state_ = false;
	has_urgent_move_state_ = false;
	input_right_ = 0;
	input_forward_ = 0;
	is_aiming_ = false;
	weapon_ = 0;
	move_speed_cap_ = MOVE_SPEED_FORWARD;
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
