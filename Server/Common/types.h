#pragma once
#include <cstdint>
#include <memory>

using RoomKey = std::uint64_t;

// 세션 하나의 연결을 식별한다. 세션 번호는 세션 배열의 위치이고, 세션 아이디는 연결마다 새로 만드는 난수라서
// 같은 위치가 재사용돼도 이전 연결과 구분된다.
struct SessionKey {
	int session_index = -1;
	int player_id = -1;
	std::uint64_t session_id = 0;

	// 같은 연결인지 비교한다. 로그인 전후로 player_id가 달라져도 같은 연결로 본다.
	bool IsSameSession(const SessionKey& other) const
	{
		return session_index == other.session_index && session_id == other.session_id;
	}

	// 로그인한 플레이어까지 같은지 비교한다.
	bool operator==(const SessionKey& other) const = default;
};
