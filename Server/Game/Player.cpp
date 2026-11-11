#include <algorithm>
#include <cmath>
#include <cstring>
#include <random>
#include "Player.h"
#include "random_seed.h"
#include "shot_spread.h"
#include "MoveSimulation.h"

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

	return true;
}

bool Player::MatchesSessionKey(SessionKey session_key) const
{
	return HasSession() && session_key_ == session_key;
}

void Player::ClearPlayer()
{
	session_key_ = {};
	ResetGameData();
}

void Player::ResetGameData()
{
	room_player_state_.store(RoomPlayerState::WAIT);

	ClearMoveState();
	ClearStun();
	action_state_.Clear();
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

ShotSpreadInput Player::GetShotSpreadInput(long long time_ms) const
{
	// 서버가 받은 상태 변화는 지연 때문에 실제와 다를 수 있어, 막 바뀐 상태는 유리한 쪽으로 본다. 조준한 채 움직이는지는 서버 이동 상태의 수평 속도로 본다.
	const bool has_state = HasMoveState();
	const MOVE_STATE state = GetMoveState();
	const bool is_falling = has_state && state.air_phase != AirPhase::GROUND && time_ms - fall_change_ms_ >= SHOT_SPREAD_STATE_ALLOWANCE_MS;
	const bool is_unaimed = !has_state || (!is_aiming_ && time_ms - aim_change_ms_ >= SHOT_SPREAD_STATE_ALLOWANCE_MS);
	return { !is_unaimed, is_falling, std::hypot(state.velocity[0], state.velocity[1]) };
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

void Player::SetMoveState(const MapData& map, const MOVE_STATE& move_state, long long time_ms)
{
	const long long state_time_ms = GetStateTimeMs(move_state, time_ms);
	const bool was_walking = server_move_.IsWalking();
	const bool is_walking = move_state.air_phase == AirPhase::GROUND;
	if (was_walking != is_walking) fall_change_ms_ = time_ms;
	clock_drift_ms_ = GetClockDrift(move_state, time_ms).value_or(0);
	has_client_time_ = true;
	if (is_walking || was_walking) {
		MOVE_STATE start = move_state;
		if (!is_walking) {
			// 공중을 시작하면 속도를 서버 상한으로 자른다. 수평 속도는 방향은 받은 값이고 크기는 그 시각 서버 이동 상태의 수평 속력 이하다.
			// 수직 속도는 점프면 점프 속도, 걸어서 떨어지면 0 이하다.
			const MOVE_STATE expected = PredictMoveState(state_time_ms);
			const float limit = std::hypot(expected.velocity[0], expected.velocity[1]);
			const float speed = std::hypot(start.velocity[0], start.velocity[1]);
			if (speed > limit) {
				start.velocity[0] *= limit / speed;
				start.velocity[1] *= limit / speed;
			}
			start.velocity[2] = std::min(start.velocity[2], move_state.air_phase == AirPhase::RISING ? JUMP_Z_VELOCITY : 0.0f);
		}
		server_move_ = MoveSimulation(map, start);
		server_move_time_ms_ = state_time_ms;
	}
	else {
		server_move_.SetView(move_state.yaw, move_state.view_pitch);
	}
	move_time_ms_ = time_ms;
	move_state_time_ms_ = state_time_ms;
	move_client_time_ms_ = move_state.client_time_ms;
	has_new_move_state_ = true;
	is_move_stopped_ = false;
}

void Player::AdvanceMove(long long time_ms)
{
	if (!HasMoveState() || time_ms <= server_move_time_ms_) return;
	const auto [input_right, input_forward] = GetMoveInput(time_ms);
	server_move_.Simulate(input_right, input_forward, is_aiming_, static_cast<float>(time_ms - server_move_time_ms_) / 1000.0f);
	server_move_time_ms_ = time_ms;
}

std::pair<std::int8_t, std::int8_t> Player::GetMoveInput(long long time_ms) const
{
	if (is_move_stopped_ || IsInputLocked(time_ms)) return { 0, 0 };
	return { input_right_, input_forward_ };
}

void Player::StopMove()
{
	server_move_.StopHorizontal();
	is_move_stopped_ = true;
}

void Player::SetMoveInput(std::int8_t input_right, std::int8_t input_forward)
{
	input_right_ = input_right;
	input_forward_ = input_forward;
}

void Player::SetAiming(bool is_aiming, long long time_ms)
{
	if (is_aiming != is_aiming_) aim_change_ms_ = time_ms;
	is_aiming_ = is_aiming;
	pose_.SetAiming(is_aiming);
}

void Player::SetSpawnLocation(const MapData& map, const float location[3], float yaw, long long time_ms)
{
	ClearMoveState();
	MOVE_STATE state{};
	std::memcpy(state.location, location, sizeof(state.location));
	state.yaw = yaw;
	state.air_phase = AirPhase::GROUND;
	server_move_ = MoveSimulation(map, state);
	server_move_time_ms_ = time_ms;
	move_time_ms_ = time_ms;
	relay_time_ms_ = time_ms;
}

std::optional<long long> Player::GetClockDrift(const MOVE_STATE& move_state, long long time_ms) const
{
	if (!has_client_time_) return 0;
	// 클라이언트 시각은 32비트로 돌아가므로 차이를 부호 있는 값으로 본다.
	const std::int32_t client_elapsed_ms = static_cast<std::int32_t>(move_state.client_time_ms - move_client_time_ms_);
	if (client_elapsed_ms < 0) return std::nullopt;
	return std::max(-CLOCK_MAX_BEHIND_MS, clock_drift_ms_ + client_elapsed_ms - (time_ms - move_time_ms_));
}

// 두 각도(도) 사이를 가까운 쪽으로 돌아 alpha만큼 간 각도
static float LerpAngle(float from, float to, float alpha)
{
	float delta = std::fmod(to - from, 360.0f);
	if (delta > 180.0f) delta -= 360.0f;
	if (delta < -180.0f) delta += 360.0f;
	return from + delta * alpha;
}

long long Player::GetStateTimeMs(const MOVE_STATE& move_state, long long time_ms) const
{
	if (!has_client_time_) return time_ms;
	return move_state_time_ms_ + static_cast<std::int32_t>(move_state.client_time_ms - move_client_time_ms_);
}

MOVE_STATE Player::PredictMoveState(long long time_ms) const
{
	if (time_ms >= server_move_time_ms_ || history_count_ == 0) {
		MoveSimulation simulation = server_move_;
		const auto [input_right, input_forward] = GetMoveInput(time_ms);
		simulation.Simulate(input_right, input_forward, is_aiming_, static_cast<float>(time_ms - server_move_time_ms_) / 1000.0f);
		return simulation.GetMoveState();
	}
	// 최신 기록부터 거슬러 가며 time_ms 이전 마지막 기록(before)과 그 다음 기록(after)을 찾는다. 다음 기록이 없으면 지금 서버 이동 상태다.
	const PlayerSnapshot* before = nullptr;
	const PlayerSnapshot* after = nullptr;
	for (int offset = history_count_ - 1; offset >= 0; --offset) {
		const PlayerSnapshot& snapshot = history_[(history_start_ + offset) % HISTORY_SIZE];
		if (snapshot.tick_time_ms <= time_ms) {
			before = &snapshot;
			break;
		}
		after = &snapshot;
	}
	if (!before) return after->move_state;
	const MOVE_STATE after_state = after ? after->move_state : GetMoveState();
	const long long after_ms = after ? after->tick_time_ms : server_move_time_ms_;
	if (after_ms <= before->tick_time_ms) return before->move_state;
	// 두 기록 사이를 시각 비율로 잇는다. 공중 단계는 앞 기록 것이다.
	const float alpha = static_cast<float>(time_ms - before->tick_time_ms) / static_cast<float>(after_ms - before->tick_time_ms);
	MOVE_STATE state = before->move_state;
	for (int axis = 0; axis < 3; ++axis) {
		state.location[axis] = std::lerp(before->move_state.location[axis], after_state.location[axis], alpha);
		state.velocity[axis] = std::lerp(before->move_state.velocity[axis], after_state.velocity[axis], alpha);
	}
	state.yaw = LerpAngle(before->move_state.yaw, after_state.yaw, alpha);
	state.view_pitch = std::lerp(before->move_state.view_pitch, after_state.view_pitch, alpha);
	return state;
}

void Player::MarkMoveStateRelayed(long long tick_time_ms)
{
	has_new_move_state_ = false;
	relay_time_ms_ = tick_time_ms;
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

std::uint32_t Player::StartMoveRollback(long long time_ms)
{
	move_time_ms_ = time_ms;
	// 맞춘 뒤에는 클라이언트 시각 기준을 다시 잡는다.
	has_client_time_ = false;
	clock_drift_ms_ = 0;
	// 롤백 패킷이 이 틱의 이동 상태 중계를 대신한다.
	MarkMoveStateRelayed(time_ms);
	is_awaiting_move_rollback_ = true;
	return ++move_rollback_id_;
}

void Player::CompleteMoveRollback(std::uint32_t rollback_id)
{
	if (rollback_id == move_rollback_id_) is_awaiting_move_rollback_ = false;
}

void Player::ClearMoveState()
{
	server_move_ = {};
	server_move_time_ms_ = 0;
	move_time_ms_ = 0;
	move_state_time_ms_ = 0;
	move_client_time_ms_ = 0;
	relay_time_ms_ = 0;
	clock_drift_ms_ = 0;
	has_client_time_ = false;
	has_new_move_state_ = false;
	input_right_ = 0;
	input_forward_ = 0;
	is_aiming_ = false;
	is_move_stopped_ = false;
	action_state_.ResetWeapon();
	is_awaiting_move_rollback_ = false;
	history_start_ = 0;
	history_count_ = 0;
}

void Player::RecordHistory(long long tick_time_ms)
{
	const int index = (history_start_ + history_count_) % HISTORY_SIZE;
	if (history_count_ == HISTORY_SIZE) history_start_ = (history_start_ + 1) % HISTORY_SIZE;
	else ++history_count_;
	history_[index] = { tick_time_ms, GetMoveState(), input_right_, input_forward_, is_aiming_, pose_, stun_end_ms_, GetStunProtectionMs() };
}

std::optional<RewoundState> Player::GetRewoundState(long long time_ms) const
{
	if (history_count_ == 0) return std::nullopt;
	// time_ms 이전 마지막 틱의 자세와 기절 상태를 쓴다. 이전 틱이 없으면 가장 오래된 틱이다.
	const PlayerSnapshot* tick = &history_[history_start_];
	for (int offset = history_count_ - 1; offset >= 0; --offset) {
		const PlayerSnapshot& snapshot = history_[(history_start_ + offset) % HISTORY_SIZE];
		if (snapshot.tick_time_ms <= time_ms) {
			tick = &snapshot;
			break;
		}
	}
	return RewoundState{ PredictMoveState(time_ms), &tick->pose, tick->stun_end_ms != 0 && time_ms < tick->stun_end_ms + tick->stun_protection_ms };
}
