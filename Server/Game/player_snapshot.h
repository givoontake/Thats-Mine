#pragma once
#include <cstdint>
#include "game_packets.h"
#include "CharacterPose.h"

// 틱이 끝날 때 남기는 플레이어 판정 상태. 명중 판정에서 사격 시점으로 되감을 때 쓴다.
// 이동 상태는 그 틱이 끝날 때의 서버 이동 상태이고, 이동 키 입력과 조준은 그 틱이 끝날 때의 값이다. 기절은 그때의 기절 끝 틱과 보호 틱 수다.
struct PlayerSnapshot {
	std::uint32_t tick_number;
	MOVE_STATE move_state;
	std::uint8_t move_keys;
	bool is_aiming;
	CharacterPose pose;
	std::uint32_t stun_end_tick;
	std::uint32_t stun_protection_ticks;
};

// 되감은 시점의 판정 상태. move_state는 그 시점의 서버 이동 상태이고, pose는 그 시점 이전 마지막 틱의 자세다.
// time_ms는 그 시점의 자세 애니메이션 진행 시각(방 틱 번호 × TICK_INTERVAL_MS)이다.
struct RewoundState {
	MOVE_STATE move_state;
	const CharacterPose* pose;
	bool is_stun_protected;
	long long time_ms;
};
