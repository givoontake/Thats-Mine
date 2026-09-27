#include <algorithm>
#include <utility>
#include "Matchmaker.h"
#include "GameServer.h"
#include "ServerThreadManager.h"
#include "GameThreadManager.h"
#include "GameRoom.h"
#include "Session.h"

Matchmaker::Matchmaker(GameServer& game_server)
	: game_server_(game_server)
{
	target_room_index_.fill(NO_TARGET_ROOM);
}

// 로비가 세션을 MATCHING으로 바꾼 뒤 호출한다. 등록 정보를 먼저 기록하고 마지막에 매칭 풀에 올린다.
void Matchmaker::Register(int session_index, long long tick_time_ms)
{
	if (session_index < 0 || session_index >= MAX_PLAYER_COUNT) return;
	registered_time_ms_[session_index] = tick_time_ms;
	target_room_index_[session_index] = NO_TARGET_ROOM;
	match_list_.Sync(session_index, []() { return true; });
}

// 매칭 단계 한 주기. 로비 단계 안에서 한 스레드만 호출한다.
void Matchmaker::RunCycle(long long tick_time_ms)
{
	const int match_list_end = match_list_.GetEnd();
	const int room_index_end = game_server_.GetThreadManager().GetGameThreadManager().GetRoomIndexEnd();
	std::fill(joining_counts_.begin(), joining_counts_.begin() + room_index_end, 0);

	// 1. 세션 상태로 입장 결과를 판단한다. 끝난 티켓(성공, 기한 초과, 연결 끊김 등)은 풀에서 뺀다.
	for (int match_index = 0; match_index < match_list_end; ++match_index) {
		const int session_index = match_list_.GetValue(match_index);
		if (session_index < 0) continue;
		auto* session = game_server_.FindSessionByIndex(session_index);
		const SessionState state = session ? session->GetState() : SessionState{};
		if (state.life == LifeState::ACTIVE && state.mode == ModeState::ROOM_PENDING) {
			const int target_room_index = target_room_index_[session_index];
			if (target_room_index >= 0 && target_room_index < room_index_end) ++joining_counts_[target_room_index];
			continue;
		}
		if (state.life == LifeState::ACTIVE && state.mode == ModeState::MATCHING) {
			target_room_index_[session_index] = NO_TARGET_ROOM;
			if (tick_time_ms - registered_time_ms_[session_index] < MATCH_TIMEOUT_MS) continue;
			// 기한 초과: 로비의 일부로서 로비 상태로 되돌리고 실패를 알린다.
			if (session->TryChangeMode(ModeState::MATCHING, ModeState::LOBBY)) game_server_.SendError(*session, ErrorCode::NOT_FOUND_JOINABLE_ROOM);
		}
		match_list_.Sync(session_index, []() { return false; });
	}

	// 2. 대기 중인 티켓을 방에 나눠 배정한다.
	// 앞 세션이 배정받은 방보다 앞쪽 방은 이번 주기에 빈자리가 없으므로, 다음 세션은 그 방부터 찾는다.
	// 끝까지 찾아도 빈자리가 없으면 뒤의 세션도 배정받을 수 없으므로 이번 주기를 끝낸다.
	int start_room_index = 0;
	for (int match_index = 0; match_index < match_list_end && start_room_index < room_index_end; ++match_index) {
		const int session_index = match_list_.GetValue(match_index);
		if (session_index < 0) continue;
		auto* session = game_server_.FindSessionByIndex(session_index);
		if (!session || session->GetLifeState() != LifeState::ACTIVE || session->GetModeState() != ModeState::MATCHING) continue;
		start_room_index = TryAssignRoom(*session, session_index, start_room_index, room_index_end);
	}
}

// 빈자리(정원 − 방 인원 − 이미 보낸 요청 수)가 있는 공개 대기방에 입장 요청을 보낸다.
// 방 정보는 원자 변수만 읽는다. 그사이 방이 바뀌었으면 방이 입장 요청을 거절하고, 세션은 MATCHING으로 돌아와 다음 주기에 다시 배정된다.
// 반환값은 다음 세션이 탐색을 시작할 방 번호다. 빈자리가 있는 방이 없으면 room_index_end를 반환한다.
int Matchmaker::TryAssignRoom(Session& session, int session_index, int start_room_index, int room_index_end)
{
	for (int room_index = start_room_index; room_index < room_index_end; ++room_index) {
		auto* room = game_server_.GetRoomByIndex(room_index);
		if (!room || room->GetRoomState() != RoomState::WAIT || room->IsPrivate()) continue;
		if (room->GetCurrentPlayerCount() + joining_counts_[room_index] >= MAX_ROOM_PLAYER_COUNT) continue;
		const RoomKey room_key = room->GetRoomKey();
		if (room_key == 0) continue;

		// 전환 대기로 먼저 바꾼 뒤 입장 요청을 넣는다.
		// 세션 상태가 바뀌어 전환하지 못한 것은 방 때문이 아니므로, 다음 세션은 이 방부터 다시 찾는다.
		if (!session.TryChangeMode(ModeState::MATCHING, ModeState::ROOM_PENDING)) return room_index;
		RoomTask task;
		task.task_type = RoomTaskType::JOIN;
		task.session_key = session.GetSessionKey();
		task.room_key = room_key;
		task.is_matching = true;
		if (!room->AddRoomTask(std::move(task))) {
			// 방이 삭제 단계에 들어가 요청을 받지 않았다. 매칭 상태로 되돌리고 다음 방을 본다.
			session.TryChangeMode(ModeState::ROOM_PENDING, ModeState::MATCHING);
			continue;
		}
		target_room_index_[session_index] = room_index;
		++joining_counts_[room_index];
		return room_index;
	}
	return room_index_end;
}
