#pragma once
#include <array>
#include "ActiveList.h"
#include "settings.h"
#include "types.h"

class GameServer;
class Session;
struct MatchingTask;

// 빠른 매칭 처리. 매칭 스레드가 소유하고, 매칭 단계마다 매칭 작업을 처리한 뒤 RunCycle을 실행한다.
// 매칭 중인 세션(MATCHING)은 매칭 스레드가 맡으며, 그동안 받은 패킷은 매칭 취소만 처리하고 나머지는 버린다.
// 방에 입장 요청을 보낼 때 풀에서 빼고 방 전환 대기(ROOM_PENDING)로 바꾼다. 방이 거절하면 방이 매칭으로 되돌리는 요청을 보낸다.
class Matchmaker
{
	static constexpr long long MATCH_TIMEOUT_MS = 5000;

	GameServer& game_server_;
	// 매칭 풀: 매칭 스레드가 맡은 세션 번호. 매칭 스레드 하나만 바꾼다.
	// 풀에 있는 세션은 매칭 스레드만 정리하므로 그동안 다른 연결로 재사용되지 않는다.
	ActiveList<MAX_PLAYER_COUNT> match_list_;
	// 세션 번호별 매칭 등록 시각. 로비에서 등록할 때 기록하고, 방 입장에 실패해 돌아오면 그대로 둬 기한은 처음 등록부터 센다.
	std::array<long long, MAX_PLAYER_COUNT> registered_time_ms_{};
	// 방 번호별로 이번 주기에 보낸 입장 요청 수. 매칭 주기마다 순회 경계까지 0으로 초기화하고 다시 센다.
	// 앞 주기에 보낸 요청은 방이 처리하면 방 인원에 들어가고, 처리하기 전에 자리가 차면 방이 거절해 매칭으로 돌아온다.
	// 주기마다 지역 배열로 만들면 방 수가 늘어날 때 스택 사용량도 같이 늘어나므로 멤버로 둔다.
	std::array<int, MAX_ROOM_COUNT> joining_counts_{};

	void ConfirmEnter(Session& session, int session_index);
	void ProcessPacket(const MatchingTask& task);
	int TryAssignRoom(Session& session, int session_index, int start_room_index, int room_index_end);

public:
	explicit Matchmaker(GameServer& game_server);

	void ProcessTask(const MatchingTask& task, long long tick_time_ms);
	void RunCycle(long long tick_time_ms);
};
