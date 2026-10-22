#pragma once
#include <array>
#include <atomic>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "types.h"
#include "settings.h"
#include "enum_class.h"
#include "game_packets.h"
#include "block_pieces.h"
#include "CharacterPose.h"
#include "hitbox_types.h"
#include "weapon_ammo.h"
#include "air_state.h"
#include "player_snapshot.h"

class MapData;

// 방 슬롯 하나를 나타낸다. 방을 처리하는 스레드만 변경하므로 슬롯 사용 여부는 세션 키 유무로 판단한다.
class Player
{
	SessionKey session_key_; // 세션 정리 후에도 방 슬롯을 식별하고, 재사용된 세션 인덱스의 오삭제를 막는다.
	char send_buffer_[BUF_SIZE];
	int send_data_size_ = 0;
	std::atomic<RoomPlayerState> room_player_state_;

	// 중계 간격 안에 여러 번 받으면 최신 이동 상태만 남긴다. 공중 단계가 바뀌었거나 서버가 이동을 멈췄으면 간격을 기다리지 않고 다음 틱에 중계한다.
	MOVE_STATE move_state_{};
	long long move_time_ms_ = 0;
	// 마지막으로 받은 이동 키 입력(-127~127), 조준 여부, 손에 든 장비(WeaponType)
	std::int8_t input_right_ = 0;
	std::int8_t input_forward_ = 0;
	bool is_aiming_ = false;
	std::uint8_t weapon_ = 0;
	// 마지막으로 인정한 위치 뒤로 받은 입력과 조준 중 가장 빠른 땅 이동 속도 상한(cm/s). 다음 위치를 검사할 때 쓴다.
	float move_speed_cap_ = MOVE_SPEED_FORWARD;
	// 조준·공중 여부가 마지막으로 바뀐 시각. 탄 퍼짐 최소치를 정할 때 막 바뀐 상태는 봐준다.
	long long aim_change_ms_ = 0;
	long long fall_change_ms_ = 0;
	bool has_new_move_state_ = false;
	// 마지막으로 인정한 위치까지 갈 수 있는 거리보다 더 움직인 거리의 누적(cm). 덜 움직이면 0까지만 줄어든다.
	float move_excess_ = 0.0f;
	// 마지막으로 인정한 상태에서 나올 수 있는 최대 속도(cm/s). 땅에서는 언리얼 가감속 식으로 따라간 수평 속도이고,
	// 공중이면 구간마다 내려간 높이만큼 키우고 올라간 만큼 줄인 3차원 속도다.
	float speed_bound_ = 0.0f;
	// 마지막으로 인정한 상태가 비탈 접촉이었는지
	bool was_on_steep_slope_ = false;
	// 서버가 판정하는 공중 상태. 마지막으로 인정한 이동 상태까지 반영한 값이다.
	AirState air_state_;
	// 허공 낙하 검사가 수직 속도를 구할 앞쪽 기준점의 높이와 클라이언트 시각. 보통 마지막으로 인정한 이동 상태 바로 앞 상태이고, 그 사이가 AIR_STEP_MIN_BASELINE_MS보다 짧으면 더 앞 상태다.
	float previous_move_z_ = 0.0f;
	std::uint32_t previous_client_time_ms_ = 0;
	// 마지막으로 인정한 이동 상태까지 이어서 어떤 면에도 닿지 않은 공중 상태의 수
	int open_air_count_ = 0;
	// 클라이언트 시각이 서버가 받은 시각보다 앞선 누적(ms). 기준이 없으면(스폰·되돌림 직후) has_client_time_이 거짓이다.
	long long clock_drift_ms_ = 0;
	bool has_client_time_ = false;
	// 명중 검증에 쓰는 서버 판정용 자세. 이동 상태와 동작으로 애니메이션 진행을 따라간다.
	CharacterPose pose_;
	// 틱마다 남긴 판정 상태. 사격 시각을 HIT_REWIND_MS까지 되감도록 그만큼의 틱과, 되감은 시각보다 앞선 틱·틱 간격이 흔들리는 몫을 고리 모양으로 둔다.
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
	std::uint32_t move_correction_id_ = 0;
	bool is_awaiting_move_correction_ = false;
	// 무기 값 순서의 탄약 상태. 게임마다 탄과 탄창 없이 시작한다.
	std::vector<WeaponAmmo> ammo_;
	// 곡괭이 사용 가능 횟수와, 최대치보다 적을 때 지금 1번이 차기 시작한 시각(ms). 다 차 있으면 0이다.
	int pickaxe_charges_ = PICKAXE_MAX_CHARGES;
	long long pickaxe_charge_start_ms_ = 0;
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
	// 지난 시간만큼 곡괭이 사용 가능 횟수를 채운다.
	void UpdatePickaxeCharges(long long time_ms);
	// 가진 아이템을 적용한 값. 아이템이 없으면 기본값이다.
	int GetPickaxeChargeMs() const;
	float GetStunGaugeDecayPerSecond() const;
	int GetStunGaugeDecayDelayMs() const;

	void ClearMoveState();



public:
	static constexpr std::uint8_t NO_ITEM_GRADE = 0xFF;

	Player();
	~Player();

	SessionKey GetSessionKey() const { return session_key_; }
	bool HasSession() const { return session_key_.session_id != 0; }
	bool MatchesSessionKey(SessionKey session_key) const;
	RoomPlayerState GetRoomPlayerState() const { return room_player_state_.load(); }
	char* GetSendBuffer() { return send_buffer_; }
	int GetSendDataSize() const { return send_data_size_; }
	const MOVE_STATE& GetMoveState() const { return move_state_; }
	long long GetMoveTimeMs() const { return move_time_ms_; }
	bool HasMoveState() const { return move_time_ms_ != 0; }
	bool WasOnSteepSlope() const { return was_on_steep_slope_; }
	// time_ms에 받은 move_state를 인정하면 될 클라이언트 시각 앞섬 누적(ms). 클라이언트 시각이 거꾸로 가면 값이 없다.
	std::optional<long long> GetClockDrift(const MOVE_STATE& move_state, long long time_ms) const;
	// 직전 두 상태가 모두 허공이었으면 move_state의 클라이언트 시각에 자유 낙하로 있어야 할 높이와 허용 오차(cm). 판정할 수 없으면 값이 없다.
	std::optional<std::pair<float, float>> GetPredictedAirZ(const MOVE_STATE& move_state) const;
	// 직전 상태에서 time_ms에 받은 move_state까지 갈 수 있는 거리(cm)와 그때의 최대 수평 속도(cm/s)
	std::pair<float, float> GetMoveReach(const MOVE_STATE& move_state, long long time_ms) const;
	// 이번 이동의 "실제 거리 - 갈 수 있는 거리"(cm)
	float GetMoveStepExcess(const MOVE_STATE& move_state, long long time_ms) const;
	// time_ms 시각에 처리하는 move_state를 인정하면 될 초과 누적(cm)
	float GetMoveExcess(const MOVE_STATE& move_state, long long time_ms) const;
	// time_ms 시각의 공중 상태. 상승 중에 정점 시각과 지연 여유가 지났으면 정점에서 하강을 시작한 것으로 바꾼다.
	AirState GetAirState(long long time_ms) const;
	// time_ms에 처리하는 move_state를 인정하면 될 공중 상태. 점프는 점프를 알린 위치와 시각에서, 걸어서 떨어지면 마지막으로 인정한 위치에서 시작하고,
	// 상승 중 하강을 알리면 그 위치에서 하강을 시작하되 정점 시각보다 늦게는 잡지 않는다. 하강 중의 상승 알림은 받지 않는다.
	// 하강 중 is_slope_slide(직전과 이번 위치가 모두 비탈 접촉)면 이번 위치와 직전 위치부터의 하강 속도로 시작점을 다시 잡는다.
	AirState GetNextAirState(const MOVE_STATE& move_state, long long time_ms, bool is_slope_slide) const;
	bool IsStunned(long long time_ms) const { return time_ms < stun_end_ms_; }
	// 기절해 이동 입력이 없는 것으로 보는지. 기절 시작 뒤 STUN_INPUT_GRACE_MS가 지나야 참이다.
	bool IsInputLocked(long long time_ms) const { return IsStunned(time_ms) && time_ms >= stun_end_ms_ - STUN_DURATION_MS + STUN_INPUT_GRACE_MS; }
	// 기절 중이거나 기절이 끝난 뒤 보호 시간 안이면 기절 게이지가 오르지 않는다.
	bool IsStunProtected(long long time_ms) const { return stun_end_ms_ != 0 && time_ms < stun_end_ms_ + GetStunProtectionMs(); }
	int GetStunProtectionMs() const;
	// 알린 기절이 time_ms에 끝났으면 한 번만 참을 돌려준다. 방이 기절 해제를 보낼 때 쓴다.
	bool TakeStunRelease(long long time_ms);
	bool IsAwaitingMoveCorrection() const { return is_awaiting_move_correction_; }
	float GetStunGauge(long long time_ms) const;
	CharacterPose& GetPose() { return pose_; }
	const CharacterPose& GetPose() const { return pose_; }
	bool CanRelayMoveState() const { return has_new_move_state_; }
	std::int8_t GetInputRight() const { return input_right_; }
	std::int8_t GetInputForward() const { return input_forward_; }
	bool IsAiming() const { return is_aiming_; }
	std::uint8_t GetWeapon() const { return weapon_; }

	void SetRoomPlayerState(RoomPlayerState new_state) { return room_player_state_.store(new_state); }

	bool InitPlayer(SessionKey session_key);
	void ClearPlayer();
	void ResetGameData();
	bool AddToSendBuffer(const char* data, int data_size);
	void ClearSendBuffer();
	// is_on_steep_slope는 move_state가 비탈 접촉인지, is_open_air는 공중이고 어떤 면에도 닿지 않았는지다.
	void SetMoveState(const MOVE_STATE& move_state, long long time_ms, bool is_on_steep_slope, bool is_open_air);
	void SetMoveInput(std::int8_t input_right, std::int8_t input_forward);
	void SetAiming(bool is_aiming, long long time_ms);
	// 손에 든 장비를 바꾼다. 판정용 자세도 꺼내기 동작을 시작한다.
	void SetWeapon(std::uint8_t weapon, long long time_ms);
	// 스폰 자리를 마지막으로 인정한 위치로 둔다. 첫 이동 상태도 이 위치와 비교해 검사한다. 스폰 패킷이 따로 가므로 중계하지 않는다.
	void SetSpawnLocation(const float location[3], float yaw, long long time_ms);
	void MarkMoveStateRelayed();
	// 기절 게이지를 올린다. 기절 중이거나 보호 중이면 무시하고, 최대치에 닿으면 기절을 시작해 true를 돌려준다.
	bool AddStunGauge(float amount, long long time_ms);
	// 이동을 되돌릴 때 수평으로 멈춘 상태로 바꿔 다른 플레이어에게 다음 틱에 바로 중계한다. 떨어지는 속도는 그대로 둔다.
	void StopMovement();
	// 힌트를 가진다. 이미 가진 힌트면 그대로 두고 거짓을 돌려준다.
	bool AddHint(std::uint32_t hint);
	// 가진 힌트 중 하나를 무작위로 골라 빼고 돌려준다. 가진 힌트가 없으면 비어 있다.
	std::optional<std::uint32_t> TakeRandomHint();
	void ResetHints() { hint_indices_.clear(); }
	// time_ms에 location으로 옮겨 수평으로 멈춘 상태로 맞추고 새 되돌리기 번호를 돌려준다. 공중이면 수직 속도 velocity_z로 air 곡선을 이어 간다.
	// 확인을 받을 때까지 이동 상태를 받지 않는다.
	std::uint32_t StartMoveCorrection(const float location[3], float velocity_z, const AirState& air, long long time_ms);
	// 기다리던 번호의 확인이면 다시 이동 상태를 받는다.
	void CompleteMoveCorrection(std::uint32_t correction_id);
	// 무기 수만큼 탄약 상태를 탄과 탄창 없이 만든다.
	void ResetAmmo(std::size_t weapon_count);
	const WeaponAmmo* GetAmmo(std::uint8_t weapon) const { return weapon < ammo_.size() ? &ammo_[weapon] : nullptr; }
	// 모든 무기의 발사 간격 검증 기준을 동기화로 보낸 서버 시각으로 채우고 보상을 비운다.
	void ResetFireBasis(long long time_ms);
	// 재장전 중이 아니고 탄이 있고 발사 간격 검증을 통과하면 탄 하나를 쓰고 인정한다. 인정하면 발사 번호와 그 발의 퍼짐 크기를 정하고 연사 누적을 늘린다.
	// time_ms는 서버가 받은 시각, action_time_ms는 클라이언트가 보낸 동작 시각이다. 인정하지 않으면 값이 없다.
	std::optional<FiredShot> TryFire(std::uint8_t weapon, const WeaponInfo& info, long long time_ms, std::uint32_t action_time_ms);
	// 재장전 중이 아니고 탄창이 가득 차 있지 않고 소지 탄이 있으면 재장전을 시작한다.
	bool TryStartReload(std::uint8_t weapon, const WeaponInfo& info, long long time_ms);
	// 재장전이 끝날 시각이 지났으면 소지 탄에서 탄창이 빈 만큼 채우고 true를 돌려준다.
	bool FinishReload(std::uint8_t weapon, const WeaponInfo& info, long long time_ms);
	// 소지 탄을 count발 늘린다.
	void AddSpareRounds(std::uint8_t weapon, int count);
	// 곡괭이 사용 가능 횟수를 최대치로 채운다.
	void ResetPickaxe();
	// 사용 가능 횟수가 있으면 1번 쓰고 인정한다. 최대치에서 쓰면 충전을 시작한다.
	bool TryUsePickaxe(long long time_ms);
	int GetPickaxeCharges() const { return pickaxe_charges_; }
	// 다음 1번이 찰 때까지 남은 시간(ms). 다 차 있으면 0이다.
	long long GetPickaxeChargeRemainingMs(long long time_ms) const;
	// 아이템을 가진다. 지금보다 높은 등급일 때만 바꾼다.
	void AddItem(ItemKind kind, std::uint8_t grade);
	// 아이템을 모두 잃는다.
	void ResetItems();
	// 블록을 하나 더 가진다. 같은 블록을 여러 개 가질 수 있다.
	void AddBlock(std::uint8_t piece) { if (block_counts_[piece] < UINT8_MAX) ++block_counts_[piece]; }
	bool CanUseBlock(std::uint8_t piece) const { return piece < BLOCK_PIECE_COUNT && block_counts_[piece] > 0; }
	// 그 블록을 하나 쓰고 그 결과로 배제된 칸을 더한다.
	void UseBlock(std::uint8_t piece, std::uint64_t excluded_cells);
	void AddKeptExcludedCells(std::uint64_t cells) { kept_excluded_cells_ |= cells; }
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
	// time_ms 시각으로 되감은 판정 상태. 위치는 서버가 받은 시각 기준 이동 상태 기록 사이를 잇는다. 남긴 틱이 없으면 값이 없다.
	// 마지막 기록 뒤면 마지막 기록과 예측할 시간(HIT_PREDICT_MAX_SECONDS까지)을 돌려주고, 예측은 PredictRewoundMove가 한다.
	std::optional<RewoundState> GetRewoundState(long long time_ms) const;
	// 되감은 상태의 마지막 기록에서 예측할 시간만큼, 그 사이 틱마다 남긴 이동 키 입력과 조준으로 이동을 이어 계산한다.
	MOVE_STATE PredictRewoundMove(const RewoundState& rewound, const MapData& map) const;
};
