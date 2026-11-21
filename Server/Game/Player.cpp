#include <algorithm>
#include <cmath>
#include <cstring>
#include <random>
#include "Player.h"
#include "random_seed.h"
#include "shot_spread.h"
#include "MoveSimulation.h"
#include "equipment_settings.h"
#include "item_settings.h"
#include "move_check_settings.h"
#include "movement_settings.h"
#include "stun_settings.h"

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
	award_reach_ticks_.fill(0);
}

void Player::AddAwardCount(AwardCategory category, std::uint32_t tick)
{
	if (category >= AwardCategory::COUNT) return;
	++award_counts_[static_cast<std::size_t>(category)];
	award_reach_ticks_[static_cast<std::size_t>(category)] = tick;
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

ShotSpreadInput Player::GetShotSpreadInput() const
{
	// 조준한 채 움직이는지는 서버 이동 상태의 수평 속도로 본다.
	const bool has_state = HasMoveState();
	const MOVE_STATE state = GetMoveState();
	const bool is_falling = has_state && state.is_falling;
	return { is_falling, std::hypot(state.velocity[0], state.velocity[1]) };
}

float Player::GetStunGaugeDecayPerSecond() const
{
	const std::uint8_t grade = item_grades_[static_cast<std::size_t>(ItemKind::STUN_GAUGE_DECAY)];
	return grade == NO_ITEM_GRADE ? STUN_GAUGE_DECAY_PER_SECOND : ITEM_STUN_GAUGE_DECAYS_PER_SECOND[grade];
}

std::uint32_t Player::GetStunGaugeDecayDelayTicks() const
{
	const std::uint8_t grade = item_grades_[static_cast<std::size_t>(ItemKind::STUN_GAUGE_DECAY_DELAY)];
	return MsToTicks(grade == NO_ITEM_GRADE ? STUN_GAUGE_DECAY_DELAY_MS : ITEM_STUN_GAUGE_DECAY_DELAYS_MS[grade]);
}

std::uint32_t Player::GetStunProtectionTicks() const
{
	const std::uint8_t grade = item_grades_[static_cast<std::size_t>(ItemKind::STUN_PROTECTION)];
	return MsToTicks(grade == NO_ITEM_GRADE ? STUN_PROTECTION_MS : ITEM_STUN_PROTECTIONS_MS[grade]);
}

void Player::AddItem(ItemKind kind, std::uint8_t grade)
{
	if (grade >= std::size(ITEM_BOX_COUNTS) || kind >= ItemKind::COUNT) return;
	// 등급 값은 높은 등급일수록 작다.
	std::uint8_t& current = item_grades_[static_cast<std::size_t>(kind)];
	if (current == NO_ITEM_GRADE || grade < current) current = grade;
	// 곡괭이 충전 아이템은 곡괭이 충전 시간을 줄인다. 더 짧아질 때만 바뀐다.
	if (kind == ItemKind::PICKAXE_CHARGE) action_state_.GetPickaxe().ShortenChargeMs(PICKAXE_SETTINGS.charge_ms - ITEM_PICKAXE_CHARGE_REDUCTIONS_MS[grade]);
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

static bool HasMoveKey(std::uint8_t move_keys, MoveKey key)
{
	return (move_keys >> static_cast<int>(key)) & 1u;
}

void Player::ApplyMoveStep(const MapData& map, const MoveSimulation& step_end, const MOVE_STATE& move_state, std::uint8_t move_keys)
{
	const std::uint8_t held_keys = static_cast<std::uint8_t>(move_keys & ~(1u << static_cast<int>(MoveKey::JUMP)));
	MoveSimulation next = step_end;
	const bool is_walking = !move_state.is_falling;
	if (is_walking || step_end.IsWalking()) {
		MOVE_STATE start = move_state;
		if (!is_walking) {
			// 공중을 시작하면 속도를 서버 상한으로 자른다. 수평 속도는 방향은 받은 값이고 크기는 서버가 계산한 번호 끝 수평 속력 이하다.
			// 수직 속도는 이 번호에 뛰었으면 점프 속도, 아니면 0 이하다.
			const MOVE_STATE expected = step_end.GetMoveState();
			const float limit = std::hypot(expected.velocity[0], expected.velocity[1]);
			const float speed = std::hypot(start.velocity[0], start.velocity[1]);
			if (speed > limit) {
				start.velocity[0] *= limit / speed;
				start.velocity[1] *= limit / speed;
			}
			start.velocity[2] = std::min(start.velocity[2], HasMoveKey(move_keys, MoveKey::JUMP) ? JUMP_Z_VELOCITY : 0.0f);
		}
		next = MoveSimulation(map, start);
	}
	// 지난 틱 기록은 고치지 않는다.
	server_move_ = next;
	move_keys_ = held_keys;
	is_landing_stop_ = false;
	network_state_ = NetworkState::NORMAL;
}

void Player::AdvanceMove(long long elapsed_ms)
{
	if (network_state_ != NetworkState::STOPPED || elapsed_ms <= 0) return;
	server_move_.Simulate(0, 0, IsAiming(), static_cast<float>(elapsed_ms) / 1000.0f, false);
	// 공중에서 멈춘 플레이어는 착지하면 땅에서 멈출 때처럼 수평으로 세운다.
	if (is_landing_stop_ && server_move_.IsWalking()) {
		server_move_.StopHorizontal();
		is_landing_stop_ = false;
	}
}

// 눌린 키로 한 축의 이동 입력(-127~127)을 만든다. 양쪽 키를 다 누르면 0이다.
static std::int8_t GetKeyAxis(std::uint8_t move_keys, MoveKey positive, MoveKey negative)
{
	const int value = static_cast<int>((move_keys >> static_cast<int>(positive)) & 1u) - static_cast<int>((move_keys >> static_cast<int>(negative)) & 1u);
	return static_cast<std::int8_t>(value * 127);
}

std::pair<std::int8_t, std::int8_t> Player::GetStepInput(std::uint8_t move_keys, std::uint32_t tick) const
{
	if (IsInputLocked(tick)) return { 0, 0 };
	return { GetKeyAxis(move_keys, MoveKey::RIGHT, MoveKey::LEFT), GetKeyAxis(move_keys, MoveKey::FORWARD, MoveKey::BACKWARD) };
}

MoveSimulation Player::PredictMoveStep(const MOVE_STATE& move_state, std::uint8_t move_keys, std::uint32_t tick) const
{
	MoveSimulation simulation = server_move_;
	simulation.SetView(move_state.yaw, move_state.view_pitch);
	if (HasMoveKey(move_keys, MoveKey::JUMP) && simulation.IsWalking() && !IsInputLocked(tick)) simulation.StartJump();
	const auto [input_right, input_forward] = GetStepInput(move_keys, tick);
	simulation.Simulate(input_right, input_forward, IsAiming(), static_cast<float>(MOVE_STEP_MS) / 1000.0f, true);
	return simulation;
}

void Player::StopMove()
{
	move_keys_ = 0;
	if (server_move_.IsWalking()) server_move_.StopHorizontal();
	else is_landing_stop_ = true;
	network_state_ = NetworkState::STOPPED;
}

bool Player::SetAiming(bool is_aiming)
{
	if (!action_state_.GetHeldEquipment().TrySetAiming(is_aiming)) return false;
	pose_.SetAiming(is_aiming);
	return true;
}

void Player::SetSpawnLocation(const MapData& map, const float location[3], float yaw, long long time_ms, std::uint32_t tick_number)
{
	ClearMoveState();
	MOVE_STATE state{};
	std::memcpy(state.location, location, sizeof(state.location));
	state.yaw = yaw;
	// 스폰 자리 바닥에 닿아 있으면 걷는 중, 아니면 떨어지는 중으로 둔다.
	state.is_falling = !MoveSimulation(map, state).IsOnWalkableFloor();
	server_move_ = MoveSimulation(map, state);
	move_time_ms_ = time_ms;
	frame_gap_.Start(tick_number);
	MarkMoveStateRelayed();
	network_state_ = NetworkState::SPAWNING;
}

void Player::CompleteSpawn(long long time_ms)
{
	if (network_state_ != NetworkState::SPAWNING) return;
	// 클라이언트가 실제로 스폰한 때부터 이동 끊김을 센다.
	move_time_ms_ = time_ms;
	network_state_ = NetworkState::NORMAL;
}

// 두 각도(도) 사이를 가까운 쪽으로 돌아 alpha만큼 간 각도
static float LerpAngle(float from, float to, float alpha)
{
	float delta = std::fmod(to - from, 360.0f);
	if (delta > 180.0f) delta -= 360.0f;
	if (delta < -180.0f) delta += 360.0f;
	return from + delta * alpha;
}

bool Player::HasMoveStateChanged() const
{
	const MOVE_STATE state = GetMoveState();
	return std::memcmp(&state, &relayed_move_state_, sizeof(MOVE_STATE)) != 0;
}

void Player::ClearStun()
{
	stun_gauge_ = 0.0f;
	stun_gauge_tick_ = 0;
	stun_end_tick_ = 0;
	has_pending_stun_release_ = false;
}

float Player::GetStunGauge(std::uint32_t tick) const
{
	if (IsStunned(tick)) return STUN_GAUGE_MAX;
	const std::uint32_t decay_start_tick = stun_gauge_tick_ + GetStunGaugeDecayDelayTicks();
	const std::uint32_t decay_ticks = tick > decay_start_tick ? tick - decay_start_tick : 0;
	return std::max(0.0f, stun_gauge_ - GetStunGaugeDecayPerSecond() * decay_ticks * TICK_INTERVAL_MS / 1000.0f);
}

// 기절하면 끝나는 틱에 게이지가 0이 되도록 기록해 두어, 끝난 뒤부터 0에서 다시 계산한다.
bool Player::AddStunGauge(float amount, std::uint32_t tick)
{
	if (IsStunProtected(tick)) return false;
	stun_gauge_ = GetStunGauge(tick) + amount;
	stun_gauge_tick_ = tick;
	if (stun_gauge_ < STUN_GAUGE_MAX) return false;
	stun_end_tick_ = tick + MsToTicks(STUN_DURATION_MS);
	stun_gauge_ = 0.0f;
	stun_gauge_tick_ = stun_end_tick_;
	has_pending_stun_release_ = true;
	return true;
}

bool Player::TakeStunRelease(std::uint32_t tick)
{
	if (!has_pending_stun_release_ || IsStunned(tick)) return false;
	has_pending_stun_release_ = false;
	return true;
}

MOVE_STATE Player::StartMoveRollback()
{
	// 롤백 패킷이 이 틱의 이동 상태 중계를 대신한다.
	MarkMoveStateRelayed();
	network_state_ = NetworkState::ROLLBACK;
	return server_move_.GetMoveState();
}

void Player::CompleteMoveRollback(long long time_ms)
{
	if (network_state_ != NetworkState::ROLLBACK) return;
	// 롤백된 클라이언트는 이 상태를 지금 이동 상태로 세팅하고 확인 뒤 번호를 이어 보내므로, 그 첫 이동 패킷에서 갭을 다시 정한다.
	frame_gap_.Restart();
	// 확인은 클라이언트가 이동 패킷보다 먼저 보낼 수 있으므로, 확인을 받은 때부터 이동 끊김을 다시 센다.
	move_time_ms_ = time_ms;
	network_state_ = NetworkState::NORMAL;
}

void Player::ClearMoveState()
{
	server_move_ = {};
	move_time_ms_ = 0;
	frame_gap_ = {};
	relayed_move_state_ = {};
	is_landing_stop_ = false;
	move_keys_ = 0;
	network_state_ = NetworkState::NORMAL;
	action_state_.ResetWeapon();
	history_start_ = 0;
	history_count_ = 0;
}

void Player::RecordHistory(std::uint32_t tick_number)
{
	const int index = (history_start_ + history_count_) % HISTORY_SIZE;
	if (history_count_ == HISTORY_SIZE) history_start_ = (history_start_ + 1) % HISTORY_SIZE;
	else ++history_count_;
	history_[index] = { tick_number, GetMoveState(), move_keys_, IsAiming(), pose_, stun_end_tick_, GetStunProtectionTicks() };
}

std::optional<RewoundState> Player::GetRewoundState(std::uint32_t tick_number, std::uint8_t progress) const
{
	if (history_count_ == 0) return std::nullopt;
	// tick_number 이하 마지막 틱(after)과 그 앞 틱(before)을 찾는다. tick_number 틱이 없으면(가장 최근 틱보다 새로움) 그 틱 하나로 본다.
	// 모두 tick_number보다 새로우면 가장 오래된 틱 하나로 본다. 틱 번호는 32비트로 돌아가므로 차이로 본다.
	const PlayerSnapshot* before = &history_[history_start_];
	const PlayerSnapshot* after = before;
	for (int offset = history_count_ - 1; offset >= 0; --offset) {
		const PlayerSnapshot& snapshot = history_[(history_start_ + offset) % HISTORY_SIZE];
		if (static_cast<std::int32_t>(snapshot.tick_number - tick_number) > 0) continue;
		after = &snapshot;
		before = snapshot.tick_number == tick_number && offset > 0 ? &history_[(history_start_ + offset - 1) % HISTORY_SIZE] : &snapshot;
		break;
	}
	// 두 틱 사이를 progress 비율로 잇는다. 자세와 기절은 그 시점 이전 마지막 틱 것이고, 공중 여부는 앞 틱 것이다.
	const float alpha = std::min<float>(progress, 100.0f) / 100.0f;
	MOVE_STATE state = before->move_state;
	for (int axis = 0; axis < 3; ++axis) {
		state.location[axis] = std::lerp(before->move_state.location[axis], after->move_state.location[axis], alpha);
		state.velocity[axis] = std::lerp(before->move_state.velocity[axis], after->move_state.velocity[axis], alpha);
	}
	state.yaw = LerpAngle(before->move_state.yaw, after->move_state.yaw, alpha);
	state.view_pitch = std::lerp(before->move_state.view_pitch, after->move_state.view_pitch, alpha);
	const PlayerSnapshot* tick = alpha < 1.0f ? before : after;
	const long long time_ms = static_cast<long long>((before->tick_number + (static_cast<std::int32_t>(after->tick_number - before->tick_number)) * alpha) * TICK_INTERVAL_MS);
	return RewoundState{ state, &tick->pose, tick->stun_end_tick != 0 && tick->tick_number < tick->stun_end_tick + tick->stun_protection_ticks, time_ms };
}
