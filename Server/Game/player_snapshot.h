#pragma once
#include <cstdint>
#include "game_packets.h"
#include "CharacterPose.h"

// 틱이 끝날 때 남기는 플레이어 판정 상태. 명중 판정에서 사격 시각으로 되감을 때 쓴다.
// 이동 상태는 그 틱 시각의 서버 이동 상태이고, 이동 키 입력과 조준은 그 틱이 끝날 때의 값이다. 기절은 그때의 기절 끝 시각과 보호 시간이다.
struct PlayerSnapshot {
	long long tick_time_ms;
	MOVE_STATE move_state;
	std::int8_t input_right;
	std::int8_t input_forward;
	bool is_aiming;
	CharacterPose pose;
	long long stun_end_ms;
	int stun_protection_ms;
};

// 되감은 시각의 판정 상태. move_state는 그 시각의 서버 이동 상태이고, pose는 그 시각 이전 마지막 틱의 자세다.
struct RewoundState {
	MOVE_STATE move_state;
	const CharacterPose* pose;
	bool is_stun_protected;
};
