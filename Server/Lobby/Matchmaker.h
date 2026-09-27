#pragma once
#include <array>
#include "ActiveList.h"
#include "settings.h"

class GameServer;
class Session;

// 빠른 매칭 담당. 로비 단계 안의 매칭 단계에서 한 스레드가 RunCycle을 실행한다.
// 매칭 중인 세션(MATCHING)은 로비가 담당하므로 매칭하는 동안에도 로비 패킷은 계속 처리된다.
// 매칭 단계는 로비의 세션 처리와 겹치지 않으므로, 로비의 일부로서 MATCHING → ROOM_PENDING 전환(방에 넘기기)을 할 수 있다.
// 입장 결과는 따로 받지 않고 세션 상태로 판단한다: ROOM이면 성공, MATCHING으로 돌아왔으면 실패, ROOM_PENDING이면 처리 중.
// 매칭 단계를 전용 스레드로 옮기면 이 전제가 깨지므로, 방에 넘기는 동작을 로비를 거치도록 바꿔야 한다.
class Matchmaker
{
	static constexpr long long MATCH_TIMEOUT_MS = 5000;
	static constexpr int NO_TARGET_ROOM = -1;

	GameServer& game_server_;
	// 매칭 풀: 매칭 중인 세션 번호. 여러 로비 스레드가 동시에 등록해도 안전하다.
	ActiveList<MAX_PLAYER_COUNT> match_list_;
	// 세션 번호별 매칭 등록 시각. 등록할 때 기록하고, 매칭 단계에서 기한 판정에 쓴다.
	std::array<long long, MAX_PLAYER_COUNT> registered_time_ms_{};
	// 세션 번호별로 입장 요청을 보낸 방 번호. 매칭 단계만 쓰며, 방별 요청 중 인원을 세어 빈자리 계산에 반영한다.
	std::array<int, MAX_PLAYER_COUNT> target_room_index_{};

	bool TryAssignRoom(Session& session, int session_index, std::array<int, MAX_ROOM_COUNT>& joining_counts, int room_index_end);

public:
	explicit Matchmaker(GameServer& game_server);

	void Register(int session_index, long long tick_time_ms);
	void RunCycle(long long tick_time_ms);
};
