#pragma once
#include <array>
#include <atomic>
#include <optional>
#include <string>
#include <vector>
#include "types.h"
#include "settings.h"
#include "enum_class.h"
#include "game_packets.h"
#include "CharacterPose.h"
#include "hitbox_types.h"
#include "weapon_ammo.h"

// 방 슬롯 하나를 나타낸다. 방을 처리하는 스레드만 변경하므로 슬롯 사용 여부는 세션 키 유무로 판단한다.
class Player
{
	SessionKey session_key_; // 세션 정리 후에도 방 슬롯을 식별하고, 재사용된 세션 인덱스의 오삭제를 막는다.
	char send_buffer_[BUF_SIZE];
	int send_data_size_ = 0;
	std::atomic<RoomPlayerState> room_player_state_;

	// 중계 간격 안에 여러 번 받으면 최신 이동 상태만 남긴다. 입력 변화가 섞여 있으면 간격을 기다리지 않고 다음 틱에 중계한다.
	MOVE_STATE move_state_{};
	long long move_time_ms_ = 0;
	long long last_move_relay_ms_ = 0;
	bool has_new_move_state_ = false;
	bool has_input_change_ = false;
	// 명중 검증에 쓰는 서버 판정용 자세. 이동 상태와 동작으로 애니메이션 진행을 따라간다.
	CharacterPose pose_;
	// 기절 게이지는 stun_gauge_time_ms_ 시각의 값이고, 그 뒤로는 초당 감소량만큼 줄어든 것으로 계산한다. 기절 중에는 최대치로 본다.
	float stun_gauge_ = 0.0f;
	long long stun_gauge_time_ms_ = 0;
	long long stun_end_ms_ = 0;
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
	// 이번 게임에서 얻은 아이템. 돋보기 앞 종류는 가진 가장 높은 등급이고 없으면 NO_ITEM_GRADE다. 돋보기는 등급마다 가진 개수다.
	std::array<std::uint8_t, static_cast<std::size_t>(ItemKind::MAGNIFIER)> item_grades_;
	std::array<int, std::size(MAGNIFIER_RADII)> magnifier_counts_{};
	// 진행 중인 돋보기 탐색의 등급, 시작 위치(cm, x·y), 끝나는 시각. 끝나는 시각이 0이면 탐색 중이 아니다.
	std::uint8_t search_grade_ = 0;
	float search_location_[2] = {};
	long long search_end_ms_ = 0;

	void ClearStun();
	// 재장전이 끝날 시각이 지났으면 소지 탄에서 탄창이 빈 만큼 채운다.
	void UpdateReload(WeaponAmmo& ammo, const WeaponInfo& info, long long time_ms);
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
	bool IsStunned(long long time_ms) const { return time_ms < stun_end_ms_; }
	// 기절 중이거나 기절이 끝난 뒤 보호 시간 안이면 기절 게이지가 오르지 않는다.
	bool IsStunProtected(long long time_ms) const { return stun_end_ms_ != 0 && time_ms < stun_end_ms_ + GetStunProtectionMs(); }
	int GetStunProtectionMs() const;
	bool IsAwaitingMoveCorrection() const { return is_awaiting_move_correction_; }
	float GetStunGauge(long long time_ms) const;
	CharacterPose& GetPose() { return pose_; }
	const CharacterPose& GetPose() const { return pose_; }
	bool CanRelayMoveState(long long tick_time_ms) const { return has_new_move_state_ && (has_input_change_ || tick_time_ms - last_move_relay_ms_ >= MOVE_RELAY_INTERVAL_MS); }

	void SetRoomPlayerState(RoomPlayerState new_state) { return room_player_state_.store(new_state); }

	bool InitPlayer(SessionKey session_key);
	void ClearPlayer();
	void ResetGameData();
	bool AddToSendBuffer(const char* data, int data_size);
	void ClearSendBuffer();
	void SetMoveState(const MOVE_STATE& move_state, long long time_ms, bool is_input_change);
	void MarkMoveStateRelayed(long long tick_time_ms);
	// 기절 게이지를 올린다. 기절 중이거나 보호 중이면 무시하고, 최대치에 닿으면 기절을 시작해 true를 돌려준다.
	bool AddStunGauge(float amount, long long time_ms);
	// 기절을 시작할 때 이동을 멈춘 상태로 바꿔 다른 플레이어에게 다음 틱에 바로 중계한다. 떨어지는 속도는 그대로 둔다.
	void StopMovement();
	// 힌트를 가진다. 이미 가진 힌트면 그대로 둔다.
	void AddHint(std::uint32_t hint);
	// 가진 힌트 중 하나를 무작위로 골라 빼고 돌려준다. 가진 힌트가 없으면 비어 있다.
	std::optional<std::uint32_t> TakeRandomHint();
	void ResetHints() { hint_indices_.clear(); }
	// 마지막으로 인정한 위치에 멈춘 상태로 되돌리고 새 되돌리기 번호를 돌려준다. 확인을 받을 때까지 이동 상태를 받지 않는다.
	std::uint32_t StartMoveCorrection();
	// 기다리던 번호의 확인이면 다시 이동 상태를 받는다.
	void CompleteMoveCorrection(std::uint32_t correction_id);
	// 무기 수만큼 탄약 상태를 탄과 탄창 없이 만든다.
	void ResetAmmo(std::size_t weapon_count);
	const WeaponAmmo* GetAmmo(std::uint8_t weapon) const { return weapon < ammo_.size() ? &ammo_[weapon] : nullptr; }
	// 재장전 중이 아니고 탄이 있고 발사 간격이 지났으면 탄 하나를 쓰고 인정한다.
	bool TryFire(std::uint8_t weapon, const WeaponInfo& info, long long time_ms);
	// 재장전 중이 아니고 탄창이 가득 차 있지 않고 소지 탄이 있으면 재장전을 시작한다.
	bool TryStartReload(std::uint8_t weapon, const WeaponInfo& info, long long time_ms);
	// 인정한 발사 하나를 명중이나 선물 열기에 쓴다. 쓸 발사가 없으면 false다.
	bool ClaimShot(std::uint8_t weapon);
	// 소지 탄을 count발 늘린다.
	void AddSpareRounds(std::uint8_t weapon, int count, const WeaponInfo& info, long long time_ms);
	// 곡괭이 사용 가능 횟수를 최대치로 채운다.
	void ResetPickaxe();
	// 사용 가능 횟수가 있으면 1번 쓰고 인정한다. 최대치에서 쓰면 충전을 시작한다.
	bool TryUsePickaxe(long long time_ms);
	int GetPickaxeCharges() const { return pickaxe_charges_; }
	// 다음 1번이 찰 때까지 남은 시간(ms). 다 차 있으면 0이다.
	long long GetPickaxeChargeRemainingMs(long long time_ms) const;
	// 아이템을 가진다. 돋보기는 개수를 늘리고, 다른 종류는 지금보다 높은 등급일 때만 바꾼다.
	void AddItem(ItemKind kind, std::uint8_t grade);
	// 아이템을 모두 잃고 진행 중인 탐색을 취소한다.
	void ResetItems();
	// 그 등급의 돋보기가 있고 탐색 중이 아니면 지금 위치에서 탐색을 시작한다.
	bool StartSearch(std::uint8_t grade, long long time_ms);
	void CancelSearch() { search_end_ms_ = 0; }
	bool IsSearchDue(long long time_ms) const { return search_end_ms_ != 0 && time_ms >= search_end_ms_; }
	std::uint8_t GetSearchGrade() const { return search_grade_; }
	const float* GetSearchLocation() const { return search_location_; }
	// 끝난 탐색을 마친다. 시작 위치에서 MAGNIFIER_MOVE_TOLERANCE 넘게 벗어나지 않았으면 돋보기 하나를 쓰고 true를 돌려준다.
	bool FinishSearch();
};
