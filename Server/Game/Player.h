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
#include "FrameGap.h"

class MapData;

// 방 슬롯 하나를 나타낸다. 방을 처리하는 스레드만 변경하므로 슬롯 사용 여부는 세션 키 유무로 판단한다.
class Player
{
	SessionKey session_key_; // 세션 정리 후에도 방 슬롯을 식별하고, 재사용된 세션 인덱스의 오삭제를 막는다.
	std::atomic<RoomPlayerState> room_player_state_;

	// 서버가 정한 이동 상태. 마지막으로 처리한 전송 프레임 번호의 이동 패킷까지 계산한 상태다.
	// 다른 플레이어에게 중계하고 판정에 쓰는 위치는 모두 이 상태다.
	MoveSimulation server_move_;
	// 마지막으로 이동 패킷을 받은 서버 시각. 처리하지 않고 버린 패킷도 센다. 이동 끊김에 쓴다.
	long long move_time_ms_ = 0;
	// 전송 프레임 번호와 방 틱의 대응
	FrameGap frame_gap_;
	// 마지막으로 다른 플레이어에게 알린 이동 상태. 틱마다 이것과 다를 때만 중계한다.
	MOVE_STATE relayed_move_state_{};
	// 마지막으로 처리한 이동 패킷의 이동 키. MoveKey 값 자리의 비트이고 점프 비트는 남기지 않는다.
	std::uint8_t move_keys_ = 0;
	// 이동 동기화 상태. STOPPED 동안은 이동 키 입력 없이 진행한다.
	NetworkState network_state_ = NetworkState::NORMAL;
	// 공중에서 멈췄는지. 궤적을 이어 가다가 착지하면 수평으로 세운다.
	bool is_landing_stop_ = false;
	// 명중 검증에 쓰는 서버 판정용 자세. 이동 상태와 동작으로 애니메이션 진행을 따라간다.
	CharacterPose pose_;
	// 틱마다 남긴 판정 상태. 사격 시점을 HIT_REWIND_MS까지 되감도록 그만큼의 틱과, 되감은 틱의 앞 틱·틱 간격이 흔들리는 몫을 고리 모양으로 둔다.
	static constexpr int HISTORY_SIZE = HIT_REWIND_MS / TICK_INTERVAL_MS + 2;
	std::array<PlayerSnapshot, HISTORY_SIZE> history_{};
	int history_start_ = 0;
	int history_count_ = 0;
	// 기절 게이지는 stun_gauge_tick_ 틱의 값이고, 그 뒤로는 초당 감소량만큼 줄어든 것으로 계산한다. 기절 중에는 최대치로 본다. 기절은 stun_end_tick_ 틱에 끝난다.
	float stun_gauge_ = 0.0f;
	std::uint32_t stun_gauge_tick_ = 0;
	std::uint32_t stun_end_tick_ = 0;
	// 기절을 알린 뒤 아직 기절 해제를 알리지 않았는지
	bool has_pending_stun_release_ = false;
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
	// 이번 게임의 시상식 부문별 기록과 그 기록에 도달한 방 틱 번호. AwardCategory 순서다.
	std::array<std::uint32_t, static_cast<std::size_t>(AwardCategory::COUNT)> award_counts_{};
	std::array<std::uint32_t, static_cast<std::size_t>(AwardCategory::COUNT)> award_reach_ticks_{};

	void ClearStun();
	// 가진 아이템을 적용한 값. 아이템이 없으면 기본값이다.
	float GetStunGaugeDecayPerSecond() const;
	std::uint32_t GetStunGaugeDecayDelayTicks() const;

	void ClearMoveState();
	// move_keys로 방 틱 tick에 처리하는 번호를 계산할 이동 입력(오른쪽, 앞쪽). 기절해 입력이 없는 것으로 보면 0이다.
	std::pair<std::int8_t, std::int8_t> GetStepInput(std::uint8_t move_keys, std::uint32_t tick) const;



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
	// time_ms에 이동 패킷을 받았다. 처리하지 않고 버리는 패킷도 이동 끊김을 세는 데 넣는다.
	void MarkMovePacketReceived(long long time_ms) { move_time_ms_ = time_ms; }
	FrameGap& GetFrameGap() { return frame_gap_; }
	const FrameGap& GetFrameGap() const { return frame_gap_; }
	// 서버 이동 상태에서 이동 패킷 하나의 번호(MOVE_STEP_MS)를 그 패킷의 몸 방향·시선과 move_keys, 지금 조준으로 계산한다. 점프 비트면 번호 시작에 뛴다.
	// tick은 그 번호를 처리하는 방 틱이고, 그때 기절해 있으면 입력 없이 계산한다. 받은 입력이라 바닥 끝에서 떨어진다.
	MoveSimulation PredictMoveStep(const MOVE_STATE& move_state, std::uint8_t move_keys, std::uint32_t tick) const;
	bool IsStunned(std::uint32_t tick) const { return tick < stun_end_tick_; }
	// 기절해 이동 입력이 없는 것으로 보는지. 기절 시작부터 끝까지 참이다.
	bool IsInputLocked(std::uint32_t tick) const { return IsStunned(tick) && tick >= stun_end_tick_ - MsToTicks(STUN_DURATION_MS); }
	// 기절 중이거나 기절이 끝난 뒤 보호 시간 안이면 기절 게이지가 오르지 않는다.
	bool IsStunProtected(std::uint32_t tick) const { return stun_end_tick_ != 0 && tick < stun_end_tick_ + GetStunProtectionTicks(); }
	std::uint32_t GetStunProtectionTicks() const;
	// 알린 기절이 방 틱 tick에 끝났으면 한 번만 참을 돌려준다. 방이 기절 해제를 보낼 때 쓴다.
	bool TakeStunRelease(std::uint32_t tick);
	NetworkState GetNetworkState() const { return network_state_; }
	float GetStunGauge(std::uint32_t tick) const;
	CharacterPose& GetPose() { return pose_; }
	const CharacterPose& GetPose() const { return pose_; }
	// 지금 이동 상태가 마지막으로 알린 이동 상태와 다른지
	bool HasMoveStateChanged() const;
	// 손에 든 장비가 조준 중인지
	bool IsAiming() const { return action_state_.GetHeldEquipment().IsAiming(); }
	// 눌린 이동 키를 모두 뗀 것으로 하고 STOPPED가 된다. 땅이면 서버 이동 상태를 수평으로 바로 세우고, 공중이면 궤적을 이어 가다가 착지할 때 세운다.
	void StopMove();
	PlayerActionState& GetActionState() { return action_state_; }
	const PlayerActionState& GetActionState() const { return action_state_; }
	// 쏠 때 퍼짐 크기를 정할 이동 상태
	ShotSpreadInput GetShotSpreadInput() const;

	void SetRoomPlayerState(RoomPlayerState new_state) { return room_player_state_.store(new_state); }

	bool InitPlayer(SessionKey session_key);
	void ClearPlayer();
	void ResetGameData();
	// 인정한 이동 패킷을 처리하고 NORMAL이 된다. step_end는 PredictMoveStep으로 계산한 이 패킷의 번호 끝 상태다.
	// 서버가 계산한 번호 끝이 땅이거나 받은 상태가 땅이면 받은 상태로 맞추고, 공중을 시작하면 속도를 서버 상한으로 자른 받은 상태로 맞춘다.
	// 둘 다 공중이면 서버가 계산한 궤적을 쓴다.
	void ApplyMoveStep(const MapData& map, const MoveSimulation& step_end, const MOVE_STATE& move_state, std::uint8_t move_keys);
	// 받은 상태와 비교하지 않고 서버가 계산한 번호 끝 상태로 둔다. 기절 중에 쓴다.
	void SetServerMove(const MoveSimulation& step_end) { server_move_ = step_end; }
	// 멈춘 플레이어의 서버 이동 상태를 elapsed_ms만큼 입력 없이 진행한다. 틱마다 부른다. 바닥 끝에서 떨어지지 않고 가장자리에 선다.
	void AdvanceMove(long long elapsed_ms);
	// 눌린 이동 키를 모두 뗀 것으로 한다. 기절하면 부른다.
	void ClearMoveKeys() { move_keys_ = 0; }
	// 손에 든 장비의 조준을 바꾼다. 조준할 수 없는 장비면 받지 않는다.
	bool SetAiming(bool is_aiming);
	// 서버 이동 상태를 스폰 자리에 둔다. 그 자리 바닥에 닿아 있으면 걷는 중, 아니면 떨어지는 중이다. 첫 이동 패킷도 이 상태에서 계산한다.
	// 스폰 패킷이 따로 가므로 중계하지 않는다. 스폰 알림을 받을 때까지 SPAWNING이다. 서버 카운트는 방 틱 tick_number부터 센다.
	void SetSpawnLocation(const MapData& map, const float location[3], float yaw, long long time_ms, std::uint32_t tick_number);
	// time_ms에 받은 스폰 알림을 처리한다. SPAWNING이면 NORMAL이 되고, 이때부터 서버 이동 상태를 진행하고 이동 끊김을 센다.
	void CompleteSpawn(long long time_ms);
	// 지금 이동 상태를 알린 것으로 한다.
	void MarkMoveStateRelayed() { relayed_move_state_ = GetMoveState(); }
	// 방 틱 tick에 기절 게이지를 올린다. 기절 중이거나 보호 중이면 무시하고, 최대치에 닿으면 기절을 시작해 true를 돌려준다.
	bool AddStunGauge(float amount, std::uint32_t tick);
	// 힌트를 가진다. 이미 가진 힌트면 그대로 두고 거짓을 돌려준다.
	bool AddHint(std::uint32_t hint);
	// 가진 힌트 중 하나를 무작위로 골라 빼고 돌려준다. 가진 힌트가 없으면 비어 있다.
	std::optional<std::uint32_t> TakeRandomHint();
	void ResetHints() { hint_indices_.clear(); }
	// 클라이언트를 지금 서버 이동 상태로 롤백하기 시작하고 ROLLBACK이 된다. 보낼 상태를 돌려준다.
	// 확인 뒤 첫 이동 패킷은 이 상태에서 계산한다. 롤백 패킷이 이번 틱 이동 상태 중계를 대신한다.
	MOVE_STATE StartMoveRollback();
	// time_ms에 받은 롤백 확인을 처리한다. ROLLBACK 중이면 NORMAL이 되고, 다음 이동 패킷에서 갭을 다시 정한다. 그 밖의 상태에서 온 확인은 버린다.
	void CompleteMoveRollback(long long time_ms);
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
	// 시상식 부문 기록을 1 늘리고 도달한 방 틱을 남긴다.
	void AddAwardCount(AwardCategory category, std::uint32_t tick);
	std::uint32_t GetAwardCount(AwardCategory category) const { return award_counts_[static_cast<std::size_t>(category)]; }
	std::uint32_t GetAwardReachTick(AwardCategory category) const { return award_reach_ticks_[static_cast<std::size_t>(category)]; }
	std::uint64_t GetSentExclusionBoard() const { return sent_exclusion_board_; }
	void SetSentExclusionBoard(std::uint64_t board) { sent_exclusion_board_ = board; }
	// 블록과 배제 판을 모두 잃는다.
	void ResetBlocks();
	// 방 틱 tick_number가 끝난 판정 상태를 남긴다. 남길 자리가 없으면 가장 오래된 틱을 지운다.
	void RecordHistory(std::uint32_t tick_number);
	// 방 틱 tick_number의 앞 틱 상태에서 그 틱 상태까지 progress(0~100)만큼 간 시점으로 되감은 판정 상태. 남긴 틱이 없으면 값이 없다.
	// 남긴 틱보다 오래된 틱이면 가장 오래된 틱, 남긴 틱보다 새로운 틱이면 가장 최근 틱으로 맞춘다.
	std::optional<RewoundState> GetRewoundState(std::uint32_t tick_number, std::uint8_t progress) const;
};
