#pragma once
#include <array>
#include <atomic>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "types.h"
#include "combat_settings.h"
#include "movement_settings.h"
#include "server_settings.h"
#include "stun_settings.h"
#include "enum_class.h"
#include "game_packets.h"
#include "block_pieces.h"
#include "CharacterPose.h"
#include "hitbox_types.h"
#include "MoveSimulation.h"
#include "player_snapshot.h"
#include "PlayerActionState.h"

class MapData;

// 방 슬롯 하나를 나타낸다. 방을 처리하는 스레드만 변경하므로 슬롯 사용 여부는 세션 키 유무로 판단한다.
class Player
{
	SessionKey session_key_; // 세션 정리 후에도 방 슬롯을 식별하고, 재사용된 세션 인덱스의 오삭제를 막는다.
	std::atomic<RoomPlayerState> room_player_state_;

	// 서버가 정한 이동 상태와 그 상태의 서버 시각(ms). 틱마다 그 틱의 이동 키 입력으로 진행하고, 받은 이동 상태를 인정하면 그 상태로 맞춘다.
	// 공중에서는 공중을 시작할 때 맞춘 상태 하나로 이어 간 궤적이다. 다른 플레이어에게 중계하고 판정에 쓰는 위치는 모두 이 상태다.
	MoveSimulation server_move_;
	long long server_move_time_ms_ = 0;
	// 마지막으로 이동 상태를 인정한 서버 시각(받은 시각). 이동 끊김과 클라이언트 시각 검사에 쓴다.
	long long move_time_ms_ = 0;
	// 마지막으로 인정한 상태가 나타내는 서버 시각과 그 상태의 클라이언트 시각. 받은 상태의 시각을 서버 시각으로 옮기는 기준이다.
	long long move_state_time_ms_ = 0;
	std::uint32_t move_client_time_ms_ = 0;
	// 마지막으로 이동 상태를 중계한 서버 시각
	long long relay_time_ms_ = 0;
	// 마지막으로 받은 이동 키 입력(-127~127)
	std::int8_t input_right_ = 0;
	std::int8_t input_forward_ = 0;
	// 서버가 멈춘 것으로 판단해 다른 플레이어에게 멈춤을 알렸는지. 멈춘 동안은 이동 키 입력 없이 진행하고, 이동 상태를 다시 인정하면 거짓이 된다.
	bool is_move_stopped_ = false;
	// 공중 여부가 마지막으로 바뀐 시각. 탄 퍼짐 최소치를 정할 때 막 바뀐 상태는 봐준다.
	long long fall_change_ms_ = 0;
	// 이동 상태를 인정해 간격을 기다리지 않고 이번 틱에 중계할지
	bool has_new_move_state_ = false;
	// 클라이언트 시각이 서버가 받은 시각보다 앞선 누적(ms). 기준이 없으면(스폰·되돌림 직후) has_client_time_이 거짓이다.
	long long clock_drift_ms_ = 0;
	bool has_client_time_ = false;
	// 명중 검증에 쓰는 서버 판정용 자세. 이동 상태와 동작으로 애니메이션 진행을 따라간다.
	CharacterPose pose_;
	// 틱마다 남긴 판정 상태. 사격 시각을 HIT_REWIND_MS까지 되감도록 그만큼의 틱과, 되감은 시각보다 앞선 틱·틱 간격이 흔들리는 몫을 고리 모양으로 둔다.
	// 받은 이동 상태를 그 시각의 서버 이동 상태와 비교할 때도 쓴다.
	static constexpr int HISTORY_SIZE = HIT_REWIND_MS / TICK_INTERVAL_MS + 2;
	std::array<PlayerSnapshot, HISTORY_SIZE> history_{};
	int history_start_ = 0;
	int history_count_ = 0;
	// 기절 게이지는 stun_gauge_time_ms_ 시각의 값이고, 그 뒤로는 초당 감소량만큼 줄어든 것으로 계산한다. 기절 중에는 최대치로 본다.
	float stun_gauge_ = 0.0f;
	long long stun_gauge_time_ms_ = 0;
	long long stun_end_ms_ = 0;
	// 기절을 알린 뒤 아직 기절 해제를 알리지 않았는지
	bool has_pending_stun_release_ = false;
	// 이동을 되돌린 번호와, 그 번호의 확인을 기다리는 중인지. 기다리는 동안 받은 이동 상태는 버린다.
	std::uint32_t move_rollback_id_ = 0;
	bool is_awaiting_move_rollback_ = false;
	// 가진 장비와 손에 든 장비, 동작을 받을지 판단하는 상태
	PlayerActionState action_state_;
	// 이번 게임에서 가진 힌트의 힌트 목록 번호. 같은 힌트는 한 번만 가진다.
	std::vector<std::uint32_t> hint_indices_;
	// 이번 게임에서 얻은 아이템. 종류마다 가진 가장 높은 등급이고 없으면 NO_ITEM_GRADE다.
	std::array<std::uint8_t, static_cast<std::size_t>(ItemKind::COUNT)> item_grades_;
	// 이번 게임에서 얻고 아직 쓰지 않은 블록 수. BLOCK_PIECES 번호 순서다.
	std::array<std::uint8_t, BLOCK_PIECE_COUNT> block_counts_{};
	// 블록 결과로 배제된 칸 모음. 기절해도 잃지 않는다.
	std::uint64_t kept_excluded_cells_ = 0;
	// 마지막으로 보낸 배제 판
	std::uint64_t sent_exclusion_board_ = 0;
	// 이번 게임의 시상식 부문별 기록과 그 기록에 도달한 시각(ms). AwardCategory 순서다.
	std::array<std::uint32_t, static_cast<std::size_t>(AwardCategory::COUNT)> award_counts_{};
	std::array<long long, static_cast<std::size_t>(AwardCategory::COUNT)> award_reach_ms_{};

	void ClearStun();
	// 가진 아이템을 적용한 값. 아이템이 없으면 기본값이다.
	float GetStunGaugeDecayPerSecond() const;
	int GetStunGaugeDecayDelayMs() const;

	void ClearMoveState();
	// time_ms 시각에 서버 이동 상태를 진행할 이동 키 입력(오른쪽, 앞쪽). 멈췄거나 기절해 입력이 없는 것으로 보면 0이다.
	std::pair<std::int8_t, std::int8_t> GetMoveInput(long long time_ms) const;



public:
	static constexpr std::uint8_t NO_ITEM_GRADE = 0xFF;

	Player();
	~Player();

	SessionKey GetSessionKey() const { return session_key_; }
	bool HasSession() const { return session_key_.session_id != 0; }
	bool MatchesSessionKey(SessionKey session_key) const;
	RoomPlayerState GetRoomPlayerState() const { return room_player_state_.load(); }
	// 지금 서버 이동 상태
	MOVE_STATE GetMoveState() const { return server_move_.GetMoveState(); }
	bool IsMoveWalking() const { return server_move_.IsWalking(); }
	long long GetMoveTimeMs() const { return move_time_ms_; }
	bool HasMoveState() const { return move_time_ms_ != 0; }
	// time_ms에 받은 move_state를 인정하면 될 클라이언트 시각 앞섬 누적(ms). 클라이언트 시각이 거꾸로 가면 값이 없다.
	std::optional<long long> GetClockDrift(const MOVE_STATE& move_state, long long time_ms) const;
	// time_ms에 받은 move_state가 나타내는 서버 시각. 마지막으로 인정한 상태가 나타내는 서버 시각에 클라이언트 시각 차이를 더한다. 기준이 없으면 받은 시각이다.
	long long GetStateTimeMs(const MOVE_STATE& move_state, long long time_ms) const;
	// time_ms 시각의 서버 이동 상태. 지금 상태보다 뒤면 지금 이동 키 입력으로 진행하고, 앞이면 틱마다 남긴 기록 사이를 잇는다.
	MOVE_STATE PredictMoveState(long long time_ms) const;
	bool IsStunned(long long time_ms) const { return time_ms < stun_end_ms_; }
	// 기절해 이동 입력이 없는 것으로 보는지. 기절 시작부터 끝까지 참이다.
	bool IsInputLocked(long long time_ms) const { return IsStunned(time_ms) && time_ms >= stun_end_ms_ - STUN_DURATION_MS; }
	// 기절 중이거나 기절이 끝난 뒤 보호 시간 안이면 기절 게이지가 오르지 않는다.
	bool IsStunProtected(long long time_ms) const { return stun_end_ms_ != 0 && time_ms < stun_end_ms_ + GetStunProtectionMs(); }
	int GetStunProtectionMs() const;
	// 알린 기절이 time_ms에 끝났으면 한 번만 참을 돌려준다. 방이 기절 해제를 보낼 때 쓴다.
	bool TakeStunRelease(long long time_ms);
	bool IsAwaitingMoveRollback() const { return is_awaiting_move_rollback_; }
	float GetStunGauge(long long time_ms) const;
	CharacterPose& GetPose() { return pose_; }
	const CharacterPose& GetPose() const { return pose_; }
	bool CanRelayMoveState() const { return has_new_move_state_; }
	std::int8_t GetInputRight() const { return input_right_; }
	std::int8_t GetInputForward() const { return input_forward_; }
	// 손에 든 장비가 조준 중인지
	bool IsAiming() const { return action_state_.GetHeldEquipment().IsAiming(); }
	bool IsMoveStopped() const { return is_move_stopped_; }
	// 서버 이동 상태를 수평으로 바로 세우고, 다음 이동 상태를 인정할 때까지 이동 키 입력 없이 진행한다.
	void StopMove();
	// 마지막 중계 뒤 클라이언트 이동 상태 전송 간격이 지났는지
	bool IsMoveRelayDue(long long tick_time_ms) const { return tick_time_ms - relay_time_ms_ >= MOVE_SEND_INTERVAL_MS; }
	PlayerActionState& GetActionState() { return action_state_; }
	const PlayerActionState& GetActionState() const { return action_state_; }
	// time_ms에 쏠 때 퍼짐 크기를 정할 이동 상태. 공중은 SHOT_SPREAD_STATE_ALLOWANCE_MS 넘게 이어졌을 때만 공중으로 본다.
	ShotSpreadInput GetShotSpreadInput(long long time_ms) const;

	void SetRoomPlayerState(RoomPlayerState new_state) { return room_player_state_.store(new_state); }

	bool InitPlayer(SessionKey session_key);
	void ClearPlayer();
	void ResetGameData();
	// time_ms에 받은 move_state를 인정한다. 땅 위 상태나 착지면 서버 이동 상태를 그 상태로 맞추고, 공중을 시작하면 속도를 서버 상한으로 자른 상태로 맞춘다.
	// 공중을 이어 가는 상태면 궤적은 그대로 두고 몸 방향만 바꾼다.
	void SetMoveState(const MapData& map, const MOVE_STATE& move_state, long long time_ms);
	// 서버 이동 상태를 time_ms까지 지금 이동 키 입력으로 진행한다. 틱마다, 그리고 이동 키 입력·조준이 바뀌기 직전에 그 시각까지 부른다.
	void AdvanceMove(long long time_ms);
	void SetMoveInput(std::int8_t input_right, std::int8_t input_forward);
	// 손에 든 장비의 조준을 바꾼다. 조준할 수 없는 장비면 받지 않는다.
	bool SetAiming(bool is_aiming, long long time_ms);
	// 서버 이동 상태를 스폰 자리에 선 상태로 둔다. 첫 이동 상태도 이 상태와 비교해 검사한다. 스폰 패킷이 따로 가므로 중계하지 않는다.
	void SetSpawnLocation(const MapData& map, const float location[3], float yaw, long long time_ms);
	void MarkMoveStateRelayed(long long tick_time_ms);
	// 기절 게이지를 올린다. 기절 중이거나 보호 중이면 무시하고, 최대치에 닿으면 기절을 시작해 true를 돌려준다.
	bool AddStunGauge(float amount, long long time_ms);
	// 힌트를 가진다. 이미 가진 힌트면 그대로 두고 거짓을 돌려준다.
	bool AddHint(std::uint32_t hint);
	// 가진 힌트 중 하나를 무작위로 골라 빼고 돌려준다. 가진 힌트가 없으면 비어 있다.
	std::optional<std::uint32_t> TakeRandomHint();
	void ResetHints() { hint_indices_.clear(); }
	// time_ms에 클라이언트를 서버 이동 상태로 롤백하기 시작하고 새 롤백 번호를 돌려준다. 서버 이동 상태는 그대로 이어 가고, 롤백 패킷이 이번 틱 이동 상태 중계를 대신한다.
	// 확인을 받을 때까지 이동 상태를 받지 않는다.
	std::uint32_t StartMoveRollback(long long time_ms);
	// 기다리던 번호의 확인이면 다시 이동 상태를 받는다.
	void CompleteMoveRollback(std::uint32_t rollback_id);
	// 아이템을 가진다. 지금보다 높은 등급일 때만 바꾼다.
	void AddItem(ItemKind kind, std::uint8_t grade);
	// 아이템을 모두 잃는다.
	void ResetItems();
	// 블록을 하나 더 가진다. 같은 블록을 여러 개 가질 수 있다.
	void AddBlock(std::uint8_t piece) { if (block_counts_[piece] < UINT8_MAX) ++block_counts_[piece]; }
	bool CanUseBlock(std::uint8_t piece) const { return piece < BLOCK_PIECE_COUNT && block_counts_[piece] > 0; }
	// 그 블록을 하나 쓰고 그 결과로 배제된 칸을 더한다.
	void UseBlock(std::uint8_t piece, std::uint64_t excluded_cells);
	std::uint64_t GetKeptExcludedCells() const { return kept_excluded_cells_; }
	// 시상식 부문 기록을 1 늘리고 도달 시각을 남긴다.
	void AddAwardCount(AwardCategory category, long long time_ms);
	std::uint32_t GetAwardCount(AwardCategory category) const { return award_counts_[static_cast<std::size_t>(category)]; }
	long long GetAwardReachMs(AwardCategory category) const { return award_reach_ms_[static_cast<std::size_t>(category)]; }
	std::uint64_t GetSentExclusionBoard() const { return sent_exclusion_board_; }
	void SetSentExclusionBoard(std::uint64_t board) { sent_exclusion_board_ = board; }
	// 블록과 배제 판을 모두 잃는다.
	void ResetBlocks();
	// 이번 틱이 끝난 판정 상태를 남긴다. 남길 자리가 없으면 가장 오래된 틱을 지운다.
	void RecordHistory(long long tick_time_ms);
	// time_ms 시각으로 되감은 판정 상태. 위치는 그 시각의 서버 이동 상태(PredictMoveState)다. 남긴 틱이 없으면 값이 없다.
	std::optional<RewoundState> GetRewoundState(long long time_ms) const;
};
