#include <algorithm>
#include <utility>
#include "Matchmaker.h"
#include "GameServer.h"
#include "SessionManager.h"
#include "ServerThreadManager.h"
#include "GameThreadManager.h"
#include "GameRoom.h"
#include "Session.h"
#include "matching_task.h"
#include "packet_types.h"
#include "common_packets.h"

Matchmaker::Matchmaker(GameServer& game_server)
	: game_server_(game_server)
{
}

// 매칭 작업을 처리한다. 매칭 스레드 하나만 호출하므로 매칭 풀을 바꾸는 쪽은 하나다.
// 이 작업을 처리할 때까지 매칭 스레드가 세션의 주체이므로, 찾은 세션이 처리하는 동안 바뀌지 않는다.
void Matchmaker::ProcessTask(const MatchingTask& task, long long tick_time_ms)
{
	if (task.task_type == MatchingTaskType::PACKET) {
		ProcessPacket(task);
		return;
	}
	auto* session = game_server_.GetSessionManager().FindSession(task.session_key);
	if (!session) return;
	const int session_index = task.session_key.session_index;
	switch (task.task_type) {
	case MatchingTaskType::ENTER:
		registered_time_ms_[session_index] = tick_time_ms;
		ConfirmEnter(*session, session_index);
		break;
	case MatchingTaskType::RETURN:
		ConfirmEnter(*session, session_index);
		break;
	// 매칭 중이던 세션의 연결 종료 정리 통보다. 풀에서 빼고 정리 가능으로 바꾼다. 이 세션에 마지막으로 하는 일이다.
	// 기한 초과나 방 배정으로 풀에서 뺐지만 옮기지 못한 세션도 소속이 그대로 MATCHING이므로 여기서 정리한다.
	case MatchingTaskType::DISCONNECT:
		match_list_.Sync(session_index, []() { return false; });
		session->SetDisconnectReady(ModeState::MATCHING);
		break;
	default:
		break;
	}
}

// 매칭 소속일 때 받은 패킷을 처리한다. 매칭 취소만 처리하고 나머지는 버린다.
// 취소는 매칭 스레드가 맡은 세션(활성이고 소속이 MATCHING)일 때만 한다. 그런 세션은 언제나 풀에 있고, 매칭 스레드만 정리하므로 처리하는 동안 바뀌지 않는다.
// 아직 등록을 확정하기 전이거나 이미 방에 보냈으면 취소할 것이 없어 버리고, 클라이언트는 매칭 결과(방 입장, 기한 초과)를 받는다.
// 풀에서 먼저 빼고 로비로 옮기는 요청을 보낸다. 취소 완료는 로비가 확정한 뒤 알린다.
// 연결 종료가 예약돼 바꾸지 못하면 소속이 MATCHING으로 남아 정리 통보로 정리한다.
void Matchmaker::ProcessPacket(const MatchingTask& task)
{
	if (reinterpret_cast<const PACKET_HEADER*>(task.packet.data())->type != C2S_CANCEL_MATCHING) return;
	auto* session = game_server_.GetSessionManager().FindSession(task.session_key);
	if (!session) return;
	const SessionState state = session->GetState();
	if (state.life != LifeState::ACTIVE || state.mode != ModeState::MATCHING) return;
	match_list_.Sync(task.session_key.session_index, []() { return false; });
	if (session->TryChangeMode(ModeState::MATCHING, ModeState::LOBBY_PENDING))
		game_server_.GetThreadManager().GetLobbyThreadManager().EnqueueTask({ LobbyTaskType::MATCHING_CANCEL, task.session_key });
}

// 매칭으로 옮기는 요청을 받는 쪽으로서 확정한다. 매칭 풀에 먼저 넣고 마지막에 소속을 바꾼다.
// 그사이 연결 종료가 예약돼 바꾸지 못하면 풀에서 다시 빼고 정리 가능으로 바꾼다. 전환 대기 중에는 정리 통보가 오지 않으므로 받는 쪽이 정리한다.
void Matchmaker::ConfirmEnter(Session& session, int session_index)
{
	match_list_.Sync(session_index, []() { return true; });
	if (session.TryChangeMode(ModeState::MATCHING_PENDING, ModeState::MATCHING)) return;
	match_list_.Sync(session_index, []() { return false; });
	session.SetDisconnectReady(ModeState::MATCHING_PENDING);
}

// 매칭 단계 한 주기. 풀의 세션마다 기한을 보고, 기한이 남았으면 방에 나눠 배정한다.
// 앞 세션이 배정받은 방보다 앞쪽 방은 이번 주기에 빈자리가 없으므로, 다음 세션은 그 방부터 찾는다.
// 끝까지 찾아도 빈자리가 없으면 뒤의 세션은 기한만 본다.
void Matchmaker::RunCycle(long long tick_time_ms)
{
	const int match_list_end = match_list_.GetEnd();
	const int room_index_end = game_server_.GetThreadManager().GetGameThreadManager().GetRoomIndexEnd();
	std::fill(joining_counts_.begin(), joining_counts_.begin() + room_index_end, 0);

	int start_room_index = 0;
	for (int match_index = 0; match_index < match_list_end; ++match_index) {
		const int session_index = match_list_.GetValue(match_index);
		if (session_index < 0) continue;
		auto* session = game_server_.GetSessionManager().FindSessionByIndex(session_index);
		// 연결 종료가 예약된 세션은 정리 통보를 받아 정리한다.
		if (!session || session->GetLifeState() != LifeState::ACTIVE) continue;
		if (tick_time_ms - registered_time_ms_[session_index] >= MATCH_TIMEOUT_MS) {
			// 기한 초과: 풀에서 빼고 로비로 옮기는 요청을 보낸다. 실패는 로비가 확정한 뒤 알린다.
			// 연결 종료가 예약돼 바꾸지 못하면 소속이 MATCHING으로 남아 정리 통보로 정리한다.
			const SessionKey session_key = session->GetSessionKey();
			match_list_.Sync(session_index, []() { return false; });
			if (session->TryChangeMode(ModeState::MATCHING, ModeState::LOBBY_PENDING))
				game_server_.GetThreadManager().GetLobbyThreadManager().EnqueueTask({ LobbyTaskType::ENTER, session_key, ErrorCode::NOT_FOUND_JOINABLE_ROOM });
			continue;
		}
		if (start_room_index < room_index_end) start_room_index = TryAssignRoom(*session, session_index, start_room_index, room_index_end);
	}
}

// 빈자리(정원 − 방 인원 − 이번 주기에 보낸 요청 수)가 있는 공개 대기방에 입장 요청을 보낸다.
// 방 정보는 원자 변수만 읽는다. 그사이 방이 바뀌었으면 방이 입장 요청을 거절하고, 세션은 매칭으로 돌아와 다음 주기에 다시 배정된다.
// 반환값은 다음 세션이 탐색을 시작할 방 번호다. 빈자리가 있는 방이 없으면 room_index_end를 반환한다.
int Matchmaker::TryAssignRoom(Session& session, int session_index, int start_room_index, int room_index_end)
{
	auto& game_thread_manager = game_server_.GetThreadManager().GetGameThreadManager();
	for (int room_index = start_room_index; room_index < room_index_end; ++room_index) {
		auto* room = game_thread_manager.GetRoomByIndex(room_index);
		if (!room || room->GetRoomState() != RoomState::WAIT || room->IsPrivate()) continue;
		if (room->GetCurrentPlayerCount() + joining_counts_[room_index] >= MAX_ROOM_PLAYER_COUNT) continue;
		const RoomKey room_key = room->GetRoomKey();
		if (room_key == 0) continue;

		// 풀에서 먼저 빼고 전환 대기로 바꾼 뒤 입장 요청을 넣는다.
		// 연결 종료가 예약돼 바꾸지 못한 것은 방 때문이 아니므로, 다음 세션은 이 방부터 다시 찾는다. 이 세션은 정리 통보로 정리한다.
		RoomTask task;
		task.task_type = RoomTaskType::JOIN;
		task.session_key = session.GetSessionKey();
		task.room_key = room_key;
		task.is_matching = true;
		match_list_.Sync(session_index, []() { return false; });
		// 목적지 방을 먼저 적고 전환 대기로 바꾼다. 전환 대기 중 받은 패킷은 그 방으로 간다.
		session.SetRoom(room_index, room_key);
		if (!session.TryChangeMode(ModeState::MATCHING, ModeState::ROOM_PENDING)) return room_index;
		if (!room->AddRoomTask(std::move(task))) {
			// 방이 삭제 단계에 들어가 요청을 받지 않았다. 요청을 넘기지 못한 매칭이 전환 대기의 주체로서 매칭으로 되돌리고, 다음 주기에 다시 배정한다.
			game_server_.GetThreadManager().GetMatchingThreadManager().GetMatchingThread().ReturnToMatching(session);
			return room_index + 1;
		}
		++joining_counts_[room_index];
		return room_index;
	}
	return room_index_end;
}
