#pragma once
#include <array>
#include "ActiveList.h"
#include "settings.h"

class GameServer;
class Session;

// 빠른 매칭 처리. 로비 단계에서 요청 처리가 모두 끝난 뒤 한 스레드가 RunCycle을 실행한다.
// 매칭 중인 세션(MATCHING)은 로비가 담당하므로 매칭하는 동안에도 로비 패킷은 계속 처리된다.
// 매칭은 로비의 요청 처리와 겹치지 않으므로, 로비의 일부로서 MATCHING → ROOM_PENDING 전환(방에 넘기기)을 할 수 있다.
// 입장 결과는 따로 받지 않고 세션 상태로 판단한다: ROOM이면 성공, MATCHING으로 돌아왔으면 실패, ROOM_PENDING이면 처리 중.
// 매칭 풀은 매칭에서 방 입장을 기다리는 세션(ROOM_PENDING)도 담아야 해서 상태만으로 정해지지 않으므로, 매치메이커가 직접 넣고 뺀다.
// 연결이 끊긴 세션은 연결 종료 정리 단계가 Remove로 뺀다.
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
	// 방 번호별 예약 인원(입장 요청을 보내고 결과를 기다리는 인원). 매칭 주기마다 순회 경계까지 0으로 초기화하고 다시 센다.
	// 주기마다 지역 배열로 만들면 방 수가 늘어날 때 스택 사용량도 같이 늘어나므로 멤버로 둔다.
	std::array<int, MAX_ROOM_COUNT> joining_counts_{};

	int TryAssignRoom(Session& session, int session_index, int start_room_index, int room_index_end);

public:
	explicit Matchmaker(GameServer& game_server);

	void Register(int session_index, long long tick_time_ms);
	void Remove(int session_index);
	void RunCycle(long long tick_time_ms);
};
