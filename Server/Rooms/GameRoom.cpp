#include <algorithm>
#include <cmath>
#include <iterator>
#include <optional>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <random>
#include <utility>
#include "GameRoom.h"
#include "GameServer.h"
#include "char_buffer.h"
#include "SessionManager.h"
#include "packet_types.h"
#include "common_packets.h"
#include "room_packets.h"
#include "game_packets.h"
#include "spawn_points.h"
#include "random_seed.h"
#include "shot_spread.h"
#include "hitbox_raycast.h"
#include "hint_types.h"
#include "treasure_placement.h"
#include "room_lifecycle_tasks.h"

// 종류별 힌트 패킷 중 가장 큰 크기. 힌트를 이어 붙일 버퍼를 잡을 때 쓴다.
constexpr std::size_t MAX_HINT_PACKET_SIZE = std::max(sizeof(S2C_DIRECTION_HINT_PACKET), sizeof(S2C_COUNT_HINT_PACKET));

// 배제 판의 칸마다 칸 번호와 x·y 범위(cm)를 is_covered(cell, x0, x1, y0, y1)에 넘겨 참인 칸을 모은다.
template <typename CoverCheck>
static std::uint64_t CollectCoveredCells(const CoverCheck& is_covered)
{
	std::uint64_t cells = 0;
	for (int row = 0; row < BLOCK_GRID_SIZE; ++row) {
		const float x0 = MAP_MIN[0] + row * BLOCK_CELL_SIZE_X;
		for (int column = 0; column < BLOCK_GRID_SIZE; ++column) {
			const float y1 = MAP_MAX[1] - column * BLOCK_CELL_SIZE_Y;
			const int cell = row * BLOCK_GRID_SIZE + column;
			if (is_covered(cell, x0, x0 + BLOCK_CELL_SIZE_X, y1 - BLOCK_CELL_SIZE_Y, y1)) cells |= 1ULL << cell;
		}
	}
	return cells;
}

GameRoom::~GameRoom()
{
}

// 서버가 방 배열을 만들 때 한 번 호출한다. 방 번호는 배열 위치이므로 바뀌지 않는다.
void GameRoom::AttachToServer(GameServer* server, int room_index)
{
	server_ = server;
	room_index_ = room_index;
}

// 빈 자리를 새 방 생성에 차지한다. 방 번호 뮤텍스 안에서 호출된다.
bool GameRoom::TryClaim()
{
	RoomState expected_state = RoomState::EMPTY;
	return room_state_.compare_exchange_strong(expected_state, RoomState::INITIALIZING);
}

// 차지한 자리에 새 방 정보를 기록하고, 마지막에 새 방 키를 공개한다.
void GameRoom::Initialize(const RoomInitData& data)
{
	memcpy(room_name_, data.room_name, sizeof(room_name_));
	memcpy(room_password_, data.room_password, sizeof(room_password_));
	is_private_.store(room_password_[0] != 0);
	host_id_ = -1;
	current_player_count_.store(0);
	room_key_.store(data.room_key, std::memory_order_release);
}

// 삭제 대기 중인 방을 초기화하기 위해 처리권을 가져온다. 이전 틱이 처리 중이면 실패하고, 호출한 쪽이 다음에 다시 시도한다.
bool GameRoom::TryBeginReset()
{
	RoomProcessState expected_state = RoomProcessState::COMPLETE;
	return processing_state_.compare_exchange_strong(expected_state, RoomProcessState::FREE);
}

// 방 자리를 비운다. 처리 상태가 FREE인 동안(처리 중인 게임 스레드가 없을 때)만 호출한다.
// 방 키를 먼저 0으로 바꿔, 방 정보를 읽던 쪽이 전후 키 비교로 재사용을 알아채게 한다.
void GameRoom::Reset()
{
	room_key_.store(0, std::memory_order_relaxed);
	std::atomic_thread_fence(std::memory_order_release);
	memset(room_name_, 0, sizeof(room_name_));
	memset(room_password_, 0, sizeof(room_password_));
	is_private_.store(false);
	for (auto& room_player : room_players_) room_player.ClearPlayer();
	host_id_ = -1;
	current_player_count_.store(0);
	pending_relays_.clear();
	room_state_.store(RoomState::EMPTY);
}

// 방 정보를 복사한다. 읽는 도중 방이 초기화·재사용됐으면 방 키가 달라지므로 false를 반환하고, 복사한 값은 버린다.
bool GameRoom::TryGetRoomInfoSnapshot(RoomInfoSnapshot& snapshot) const
{
	const RoomKey room_key = room_key_.load(std::memory_order_acquire);
	if (room_key == 0) return false;
	snapshot.room_key = room_key;
	snapshot.room_state = room_state_.load();
	snapshot.current_player_count = static_cast<int>(current_player_count_.load());
	memcpy(snapshot.room_name, room_name_, sizeof(snapshot.room_name));
	snapshot.is_private = is_private_.load();
	std::atomic_thread_fence(std::memory_order_acquire);
	return room_key_.load(std::memory_order_relaxed) == room_key;
}

int GameRoom::GetCurrentPlayerCount() const
{
	return static_cast<int>(current_player_count_.load());
}

Session* GameRoom::FindSession(const Player& player) const
{
	if (!player.HasSession()) return nullptr;
	return server_->GetSessionManager().FindSession(player.GetSessionKey());
}

Player* GameRoom::FindPlayer(SessionKey session_key)
{
	auto& room_players = room_players_;
	for (auto& room_player : room_players) {
		if (room_player.MatchesSessionKey(session_key)) return &room_player;
	}
	return nullptr;
}

// 이 방이 담당 중인 플레이어인지 확인한다. 담당 상태가 ROOM이고 이 방 슬롯에 있으면 이 방 소속이다.
bool GameRoom::IsOwnedPlayer(const Session& session, SessionKey session_key)
{
	return session.GetModeState() == ModeState::ROOM && FindPlayer(session_key) != nullptr;
}

// 방에 소속된 세션을 로비로 넘긴다. 로비는 거절하지 않으므로 방이 로비 상태로 바로 확정하고 결과를 알린다.
// 연결 종료가 예약된 세션이면 로비로 보내지 않고 자리를 뺀 뒤 소속을 놓는다. 나머지 정리는 연결 종료 정리 단계가 한다.
void GameRoom::LeaveToLobby(SessionKey session_key, RoomExitType exit_type)
{
	if (!FindPlayer(session_key)) return;
	auto* session = server_->GetSessionManager().FindSession(session_key);
	if (!session) return;
	if (!session->TryChangeMode(ModeState::ROOM, ModeState::LOBBY)) {
		if (session->GetLifeState() != LifeState::DISCONNECTING) return;
		CompletePlayerRemoval(session_key, exit_type);
		session->ReleaseMode(ModeState::ROOM);
		return;
	}
	CompletePlayerRemoval(session_key, exit_type);
	// 방을 떠나 로비 목록에 있어야 하는 상태가 되었으므로 목록을 맞춘다.
	server_->SyncLobbyList(session_key.session_index);
	// 방 퇴장·강퇴 완료를 본인에게 알린다.
	S2C_REMOVE_PLAYER_PACKET remove_p;
	remove_p.header.size = static_cast<std::uint16_t>(sizeof(remove_p));
	remove_p.header.type = S2C_REMOVE_PLAYER;
	remove_p.player_id = session_key.player_id;
	session->SendPacket(reinterpret_cast<char*>(&remove_p), remove_p.header.size);
}

bool GameRoom::AddHostSession(SessionKey session_key)
{
	auto& room_players = room_players_;
	if (!room_players[0].InitPlayer(session_key)) return false;
	current_player_count_.store(1);
	host_id_ = session_key.player_id;
	return true;
}

// 비밀번호는 남는 부분이 0으로 채워진 고정 길이 배열이므로 배열 전체를 비교한다.
int GameRoom::AddPlayer(Session& new_session, SessionKey session_key, const char* room_password)
{
	if (new_session.GetLifeState() != LifeState::ACTIVE || new_session.GetModeState() != ModeState::ROOM_PENDING) return ErrorCode::INVALID_REQUEST;
	if (memcmp(room_password_, room_password, sizeof(room_password_)) != 0) return ErrorCode::ROOM_INVALID_PASSWORD;
	const RoomState room_state = room_state_.load();
	if (room_state == RoomState::PLAY) return ErrorCode::ROOM_IN_GAME;
	if (room_state != RoomState::WAIT) return ErrorCode::ROOM_NOT_FOUND;
	auto& room_players = room_players_;
	if (FindPlayer(session_key)) return ErrorCode::ROOM_ALREADY_JOINED;
	int added_slot = -1;
	for (int i = 0; i < room_players.size(); ++i) {
		if (room_players[i].HasSession()) continue;
		added_slot = i;
		break;
	}
	if (added_slot < 0) return ErrorCode::ROOM_FULL;

	// 방이 받는 쪽으로서 입장을 확정한다. 확정된 뒤부터 입출력 스레드가 이 방으로 패킷을 보내므로 방 번호를 먼저 적는다.
	new_session.SetRoomIndex(room_index_);
	if (!new_session.TryChangeMode(ModeState::ROOM_PENDING, ModeState::ROOM)) return ErrorCode::INVALID_REQUEST;
	room_players[added_slot].InitPlayer(session_key);
	++current_player_count_;

	if (!IsPrivate()) {
		S2C_ADD_PUBLIC_ROOM_PACKET public_p;
		public_p.header.size = static_cast<std::uint16_t>(sizeof(public_p));
		public_p.header.type = S2C_ADD_PUBLIC_ROOM;
		memcpy(public_p.room_name, room_name_, sizeof(public_p.room_name));
		new_session.SendPacket(reinterpret_cast<char*>(&public_p), public_p.header.size);
	}
	else {
		S2C_ADD_PRIVATE_ROOM_PACKET private_p;
		private_p.header.size = static_cast<std::uint16_t>(sizeof(private_p));
		private_p.header.type = S2C_ADD_PRIVATE_ROOM;
		memcpy(private_p.room_name, room_name_, sizeof(private_p.room_name));
		new_session.SendPacket(reinterpret_cast<char*>(&private_p), private_p.header.size);
	}

	// 본인의 입장을 본인 제외 나머지에게(방 생성 시 본인은 방에 추가된다)
	for (int i = 0; i < room_players.size(); ++i) {
		if (i == added_slot) continue;
		auto& room_player = room_players[i];
		auto session = FindSession(room_player);
		if (!session) continue;
		S2C_ADD_PLAYER_PACKET add_p;
		add_p.header.size = static_cast<std::uint16_t>(sizeof(add_p));
		add_p.header.type = S2C_ADD_PLAYER;
		add_p.player_id = new_session.GetSessionKey().player_id;
		StringToCharBuf(new_session.GetNickname(), add_p.nickname, sizeof(add_p.nickname));
		session->SendPacket(reinterpret_cast<char*>(&add_p), add_p.header.size);
	}

	// 본인 제외 나머지 플레이어를 본인에게
	for (int i = 0; i < room_players.size(); ++i)	{
		if (i == added_slot) continue;
		auto& room_player = room_players[i];
		auto session = FindSession(room_player);
		if (!session) continue;
		S2C_ADD_PLAYER_PACKET add_p;
		add_p.header.size = static_cast<std::uint16_t>(sizeof(add_p));
		add_p.header.type = S2C_ADD_PLAYER;
		add_p.player_id = session->GetSessionKey().player_id;
		StringToCharBuf(session->GetNickname(), add_p.nickname, sizeof(add_p.nickname));
		new_session.SendPacket(reinterpret_cast<char*>(&add_p), add_p.header.size);
		S2C_READY_PACKET ready_p{};
		ready_p.header = {sizeof(ready_p), S2C_READY};
		ready_p.player_id = add_p.player_id;
		ready_p.is_ready = room_player.GetRoomPlayerState() == RoomPlayerState::READY;
		new_session.SendPacket(reinterpret_cast<const char*>(&ready_p), sizeof(ready_p));
	}

	// 새로 입장한 세션에게 방장이 누구인지
	S2C_UPDATE_HOST_PACKET host_p;
	host_p.header.size = static_cast<std::uint16_t>(sizeof(host_p));
	host_p.header.type = S2C_UPDATE_HOST;
	host_p.new_host_id = host_id_;
	new_session.SendPacket(reinterpret_cast<char*>(&host_p), host_p.header.size);
	return SUCCESS;
}

void GameRoom::HandlePacket(char* packet, Session& request_session, long long receive_time_ms)
{
	switch (reinterpret_cast<PACKET_HEADER*>(packet)->type) {
	case C2S_READY: {
		RoomTask task;
		task.task_type = RoomTaskType::READY;
		task.session_key = request_session.GetSessionKey();
		task.is_ready = reinterpret_cast<C2S_READY_PACKET*>(packet)->is_ready;
		task.room_key = GetRoomKey();
		AddRoomTask(std::move(task));
		break;
	}
	case C2S_KICK: {
		C2S_KICK_PACKET* kick_p = reinterpret_cast<C2S_KICK_PACKET*>(packet);
		RoomTask task;
		task.task_type = RoomTaskType::KICK;
		task.session_key = request_session.GetSessionKey();
		task.target_player_id = kick_p->kick_player_id;
		task.room_key = GetRoomKey();
		AddRoomTask(std::move(task));
		break;
	}
	case C2S_REMOVE_PLAYER: {
		LeaveToLobby(request_session.GetSessionKey(), RoomExitType::LEAVE);
		break;
	}
	case C2S_START: {
		RoomTask task;
		task.task_type = RoomTaskType::START;
		task.session_key = request_session.GetSessionKey();
		task.room_key = GetRoomKey();
		AddRoomTask(std::move(task));
		break;
	}
	case C2S_MOVE: {
		// 전원 로딩을 기다리는 동안에는 움직이지 못하므로 이동 상태를 받지 않는다.
		if (room_state_.load() != RoomState::PLAY || match_phase_ == MatchPhase::LOADING) break;
		auto* room_player = FindPlayer(request_session.GetSessionKey());
		// 되돌리기 확인을 기다리는 동안 받은 이동 상태는 버린다.
		if (!room_player || room_player->GetRoomPlayerState() != RoomPlayerState::PLAY || room_player->IsAwaitingMoveCorrection()) break;
		// 기절 중에도 위치와 공중 단계는 중력과 관성으로 움직인 결과라 평소처럼 검사한다.
		const MOVE_STATE& move_state = reinterpret_cast<C2S_MOVE_PACKET*>(packet)->move_state;
		// 이동 시각은 받은 순간의 서버 시각으로 잡는다. 처리한 틱 시각을 쓰면 같은 간격으로 온 패킷도 틱 하나만큼 간격이 흔들려 빠를수록 속도 검사 오차가 커진다.
		const bool is_on_steep_slope = move_state.air_phase == AirPhase::FALLING && IsTouchingSteepSlope(move_state.location);
		const bool is_open_air = move_state.air_phase != AirPhase::GROUND && !IsTouchingSurface(*room_player, move_state.location);
		// 직전과 이번 위치가 모두 비탈 접촉이면 비탈을 타고 온 구간으로 본다.
		const MoveCheckResult result = CheckMove(*room_player, move_state, receive_time_ms, is_on_steep_slope && room_player->WasOnSteepSlope(), is_open_air);
		if (result != MoveCheckResult::ACCEPTED) {
			CorrectMove(*room_player, request_session, result == MoveCheckResult::AIR_CORRECTION);
			break;
		}
		room_player->SetMoveState(move_state, receive_time_ms, is_on_steep_slope, is_open_air);
		break;
	}
	case C2S_MOVE_INPUT:
	case C2S_AIM:
	case C2S_EQUIP: {
		if (room_state_.load() != RoomState::PLAY) break;
		auto* room_player = FindPlayer(request_session.GetSessionKey());
		if (!room_player || room_player->GetRoomPlayerState() != RoomPlayerState::PLAY) break;
		const int player_id = request_session.GetSessionKey().player_id;
		const std::uint8_t type = reinterpret_cast<PACKET_HEADER*>(packet)->type;
		// 기절 중에는 조작을 받지 않으므로 이동 키 입력과 조준을 버린다. 장비는 기절과 엇갈려 바꾼 것을 놓치지 않도록 받는다.
		if (type != C2S_EQUIP && room_player->IsStunned(tick_time_ms_)) break;
		if (type == C2S_MOVE_INPUT) {
			const auto* request_p = reinterpret_cast<C2S_MOVE_INPUT_PACKET*>(packet);
			room_player->SetMoveInput(request_p->input_right, request_p->input_forward);
			const S2C_MOVE_INPUT_PACKET input_p{ { static_cast<std::uint16_t>(sizeof(S2C_MOVE_INPUT_PACKET)), S2C_MOVE_INPUT }, player_id, request_p->input_right, request_p->input_forward };
			QueueRelay(&input_p, sizeof(input_p));
		}
		else if (type == C2S_AIM) {
			const bool is_aiming = reinterpret_cast<C2S_AIM_PACKET*>(packet)->is_aiming;
			room_player->SetAiming(is_aiming, tick_time_ms_);
			const S2C_AIM_PACKET aim_p{ { static_cast<std::uint16_t>(sizeof(S2C_AIM_PACKET)), S2C_AIM }, player_id, is_aiming };
			QueueRelay(&aim_p, sizeof(aim_p));
		}
		else {
			const std::uint8_t weapon = reinterpret_cast<C2S_EQUIP_PACKET*>(packet)->weapon;
			if (!server_->GetHitboxData().GetWeaponInfo(weapon)) break;
			room_player->SetWeapon(weapon, tick_time_ms_);
			const S2C_EQUIP_PACKET equip_p{ { static_cast<std::uint16_t>(sizeof(S2C_EQUIP_PACKET)), S2C_EQUIP }, player_id, weapon };
			QueueRelay(&equip_p, sizeof(equip_p));
		}
		break;
	}
	case C2S_MOVE_CORRECTION_ACK: {
		if (room_state_.load() != RoomState::PLAY) break;
		auto* room_player = FindPlayer(request_session.GetSessionKey());
		if (room_player) room_player->CompleteMoveCorrection(reinterpret_cast<C2S_MOVE_CORRECTION_ACK_PACKET*>(packet)->correction_id);
		break;
	}
	case C2S_ACTION:
	case C2S_RELOAD: {
		// 동작은 손에 든 장비로, 재장전은 재장전 패킷의 무기로 판정한다. 인정하지 않은 총 발사는 쏜 사람에게 알려 클라이언트가 발사 번호를 되돌리게 한다.
		auto* room_player = room_state_.load() == RoomState::PLAY ? FindPlayer(request_session.GetSessionKey()) : nullptr;
		if (!room_player || room_player->GetRoomPlayerState() != RoomPlayerState::PLAY) break;
		const bool is_reload = reinterpret_cast<PACKET_HEADER*>(packet)->type == C2S_RELOAD;
		const PlayerAction action = is_reload ? PlayerAction::RELOAD : static_cast<PlayerAction>(reinterpret_cast<C2S_ACTION_PACKET*>(packet)->action);
		const std::uint8_t weapon = is_reload ? reinterpret_cast<C2S_RELOAD_PACKET*>(packet)->weapon : room_player->GetWeapon();
		const bool can_act = action <= PlayerAction::PICKUP && (is_reload || action != PlayerAction::RELOAD) && !room_player->IsStunned(tick_time_ms_);
		if (!can_act || !AcceptAction(*room_player, request_session, action, weapon)) {
			if (action == PlayerAction::FIRE) {
				S2C_FIRE_REJECTED_PACKET rejected_p;
				rejected_p.header = { static_cast<std::uint16_t>(sizeof(rejected_p)), S2C_FIRE_REJECTED };
				rejected_p.weapon = weapon;
				request_session.SendPacket(reinterpret_cast<char*>(&rejected_p), rejected_p.header.size);
			}
			break;
		}
		const int player_id = request_session.GetSessionKey().player_id;
		if (is_reload) {
			const S2C_RELOAD_PACKET reload_p{ { static_cast<std::uint16_t>(sizeof(S2C_RELOAD_PACKET)), S2C_RELOAD }, player_id, weapon };
			QueueRelay(&reload_p, sizeof(reload_p));
		}
		else {
			const S2C_ACTION_PACKET action_p{ { static_cast<std::uint16_t>(sizeof(S2C_ACTION_PACKET)), S2C_ACTION }, player_id, static_cast<std::uint8_t>(action) };
			QueueRelay(&action_p, sizeof(action_p));
		}
		room_player->GetPose().StartAction(static_cast<std::uint8_t>(action), weapon, tick_time_ms_);
		break;
	}
	case C2S_HIT: {
		if (room_state_.load() != RoomState::PLAY) break;
		auto* room_player = FindPlayer(request_session.GetSessionKey());
		if (!room_player || room_player->GetRoomPlayerState() != RoomPlayerState::PLAY || room_player->IsStunned(tick_time_ms_)) break;
		ProcessHit(*room_player, *reinterpret_cast<C2S_HIT_PACKET*>(packet));
		break;
	}
	case C2S_BOX_OPEN:
	case C2S_BOX_SHOT: {
		if (room_state_.load() != RoomState::PLAY) break;
		auto* room_player = FindPlayer(request_session.GetSessionKey());
		if (!room_player || room_player->GetRoomPlayerState() != RoomPlayerState::PLAY || room_player->IsStunned(tick_time_ms_)) break;
		if (reinterpret_cast<PACKET_HEADER*>(packet)->type == C2S_BOX_SHOT) {
			const auto* shot_p = reinterpret_cast<C2S_BOX_SHOT_PACKET*>(packet);
			ProcessBoxOpen(*room_player, shot_p->box, shot_p->weapon);
		}
		else ProcessBoxOpen(*room_player, reinterpret_cast<C2S_BOX_OPEN_PACKET*>(packet)->box, std::nullopt);
		break;
	}
	case C2S_BLOCK_USE: {
		if (room_state_.load() != RoomState::PLAY) break;
		auto* room_player = FindPlayer(request_session.GetSessionKey());
		if (!room_player || room_player->GetRoomPlayerState() != RoomPlayerState::PLAY) break;
		ProcessBlockUse(*room_player, *reinterpret_cast<C2S_BLOCK_USE_PACKET*>(packet));
		break;
	}
	case C2S_TREASURE_REVEAL: {
		// 시험용이라 방장이 요청할 때만 플레이 중인 전원에게 보물 위치를 알려 준다. 혼자 하는 방에서는 그 사람이 방장이다.
		if (room_state_.load() != RoomState::PLAY || request_session.GetSessionKey().player_id != host_id_) break;
		auto* room_player = FindPlayer(request_session.GetSessionKey());
		if (!room_player || room_player->GetRoomPlayerState() != RoomPlayerState::PLAY) break;
		S2C_TREASURE_REVEAL_PACKET reveal_p;
		reveal_p.header = { static_cast<std::uint16_t>(sizeof(reveal_p)), S2C_TREASURE_REVEAL };
		reveal_p.location[0] = treasure_placement_.location.x;
		reveal_p.location[1] = treasure_placement_.location.y;
		reveal_p.location[2] = treasure_placement_.location.z;
		for (auto& player : room_players_) {
			auto session = FindSession(player);
			if (session && player.GetRoomPlayerState() == RoomPlayerState::PLAY) session->SendPacket(reinterpret_cast<char*>(&reveal_p), reveal_p.header.size);
		}
		break;
	}
	case C2S_AWARD_DONE: {
		// 시상식 중 끝까지 본 사람만 먼저 방으로 돌려보낸다. 방은 시상식이 끝날 때 게임 전 대기 상태로 돌아간다.
		if (room_state_.load() != RoomState::PLAY || award_end_ms_ == 0) break;
		auto* room_player = FindPlayer(request_session.GetSessionKey());
		if (!room_player || room_player->GetRoomPlayerState() != RoomPlayerState::PLAY) break;
		room_player->ResetGameData();
		S2C_RETURN_TO_ROOM_PACKET return_p;
		return_p.header = { static_cast<std::uint16_t>(sizeof(return_p)), S2C_RETURN_TO_ROOM };
		request_session.SendPacket(reinterpret_cast<char*>(&return_p), return_p.header.size);
		break;
	}
	case C2S_LOAD_COMPLETE: {
		if (room_state_.load() != RoomState::PLAY) break;
		auto* room_player = FindPlayer(request_session.GetSessionKey());
		if (room_player && room_player->GetRoomPlayerState() == RoomPlayerState::LOADING) room_player->SetRoomPlayerState(RoomPlayerState::LOADED);
		break;
	}
	default:
		break;
	}
}

// 방 키가 다르면 같은 자리에 새로 만들어진 다른 방이므로 받지 않는다.
// 생성 중인 방도 작업을 받아 쌓아 두고, 방이 열려 게임 스레드가 처리하기 시작하면 처리한다.
bool GameRoom::AddRoomTask(RoomTask task)
{
	std::lock_guard<std::mutex> lock(room_mutex_);
	const RoomState state = room_state_.load();
	if (state != RoomState::INITIALIZING && state != RoomState::WAIT && state != RoomState::PLAY) return false;
	if (task.room_key != GetRoomKey()) return false;
	room_tasks_.Enqueue(std::move(task));
	return true;
}

// 입출력 스레드가 받은 패킷은 받을 때의 세션 상태로 이 방에 왔으므로, 지금도 이 방의 플레이어인 세션의 패킷만 처리한다.
// 받은 순서대로 처리하므로 같은 틱 안에서도 플레이어 사이의 처리 순서가 도착 순서가 된다.
void GameRoom::AddPacketTask(std::unique_ptr<SessionPacketTask> task)
{
	// 방장은 방을 여는 사이에 생성 응답을 받고 패킷을 보낼 수 있으므로 생성 중인 방도 받는다.
	const RoomState state = room_state_.load();
	if (state != RoomState::INITIALIZING && state != RoomState::WAIT && state != RoomState::PLAY) return;
	packet_tasks_.Enqueue(std::move(task));
}

void GameRoom::ProcessPacketTasks()
{
	const std::size_t task_count = packet_tasks_.ClaimTaskCount();
	for (std::size_t i = 0; i < task_count; ++i) {
		const auto task = packet_tasks_.Dequeue();
		auto* session = server_->GetSessionManager().FindSession(task->session_key);
		if (!session || !IsOwnedPlayer(*session, task->session_key)) continue;
		HandlePacket(task->packet.data(), *session, task->receive_time_ms);
	}
}

// 받는 쪽으로서 입장 요청을 확정한다. 성공하면 ROOM, 실패하면 돌려보내며 모든 분기가 결과로 끝난다.
// 빠른 매칭에서 온 요청이 실패하면 매칭 상태로 되돌리고, 매치메이커가 다음 주기에 다시 배정한다.
void GameRoom::ProcessJoinTask(const RoomTask& task)
{
	// 한 세션에 걸린 전환 요청은 하나뿐이므로, 전환 대기가 아니면 이 요청의 대상이 아니다.
	auto* session = server_->GetSessionManager().FindSession(task.session_key);
	if (!session || session->GetModeState() != ModeState::ROOM_PENDING) return;

	// 다른 방으로 재사용되기 전의 입장 요청이면 방을 찾지 못한 것으로 거절한다.
	const int result = task.room_key == GetRoomKey() ? AddPlayer(*session, task.session_key, task.room_password) : ErrorCode::ROOM_NOT_FOUND;
	if (result == SUCCESS) {
		// ROOM이 되어 로비 목록에 있으면 안 되므로 목록을 맞춘다.
		server_->SyncLobbyList(task.session_key.session_index);
		return;
	}

	if (task.is_matching) {
		session->TryChangeMode(ModeState::ROOM_PENDING, ModeState::MATCHING);
		return;
	}
	// 로비는 거절하지 않으므로 방이 로비 상태로 바로 되돌리고 실패를 알린다. 방 전환 대기 중에도 로비 목록에 있었으므로 목록은 그대로다.
	if (session->TryChangeMode(ModeState::ROOM_PENDING, ModeState::LOBBY)) server_->SendError(*session, result);
}

void GameRoom::ProcessRoomTask(const RoomTask& task)
{
	if (task.task_type == RoomTaskType::JOIN) {
		ProcessJoinTask(task);
		return;
	}
	auto* session = server_->GetSessionManager().FindSession(task.session_key);
	if (!session || !IsOwnedPlayer(*session, task.session_key)) return;
	// 연결 종료 정리 단계가 보낸 요청이다. 자리를 빼고 소속을 놓으면 정리 단계가 다음 주기에 세션을 해제한다.
	if (task.task_type == RoomTaskType::DISCONNECT) {
		if (session->GetLifeState() != LifeState::DISCONNECTING) return;
		CompletePlayerRemoval(task.session_key, RoomExitType::LEAVE);
		session->ReleaseMode(ModeState::ROOM);
		return;
	}
	ProcessSpecificRoomTask(task);
}

void GameRoom::ProcessRoomTasks()
{
	const std::size_t task_count = room_tasks_.ClaimTaskCount();
	for (std::size_t i = 0; i < task_count; ++i) ProcessRoomTask(room_tasks_.Dequeue());
	TryPostRoomDelete();
}

void GameRoom::ProcessRoomTick(long long tick_time_ms)
{
	tick_time_ms_ = tick_time_ms;
	ProcessRoomTasks();
	ProcessPacketTasks();
	ProcessRoomTasks();
	if (room_state_.load() == RoomState::PLAY) ProcessGameTick(tick_time_ms);
}

// 생성이 끝난 방을 대기 상태로 열고, 처리 상태를 FREE에서 COMPLETE로 바꿔 게임 스레드가 처리하기 시작하게 한다.
void GameRoom::CompleteRoomInitialization()
{
	StoreRoomState(RoomState::WAIT);
	processing_state_.store(RoomProcessState::COMPLETE);
}

void GameRoom::BeginRoomDelete()
{
	std::lock_guard<std::mutex> lock(room_mutex_);
	StoreRoomState(RoomState::WAITING_DELETE);
}

void GameRoom::TryPostRoomDelete()
{
	{
		std::lock_guard<std::mutex> lock(room_mutex_);
		if (room_state_.load() != RoomState::WAITING_DELETE || room_tasks_.GetTaskCount() != 0) return;
		StoreRoomState(RoomState::DELETE_POST);
	}
	server_->EnqueueRoomLifecycleTask(std::make_unique<RoomLifecycleTask>(room_index_, GetRoomKey()));
}

// 게임이 끝나 방을 게임 전 대기 상태로 되돌린다. 플레이어마다 게임 데이터를 지워 준비를 풀고, 방의 모든 플레이어에게 방 복귀를 보낸다.
// 시상식을 먼저 끝내 방으로 돌아간 플레이어(게임 중인데 대기 상태)에게는 다시 보내지 않는다.
// 그 사이 마지막 플레이어가 나가 방 삭제가 시작됐으면 아무것도 하지 않는다.
void GameRoom::ClearGame()
{
	if (!TryChangeRoomState(RoomState::PLAY, RoomState::WAIT)) return;
	match_phase_ = MatchPhase::LOADING;
	game_start_ms_ = 0;
	game_end_ms_ = 0;
	award_end_ms_ = 0;
	pending_relays_.clear();
	S2C_RETURN_TO_ROOM_PACKET return_p;
	return_p.header = { static_cast<std::uint16_t>(sizeof(return_p)), S2C_RETURN_TO_ROOM };
	for (auto& room_player : room_players_) {
		auto session = FindSession(room_player);
		if (!session) continue;
		const bool has_returned = room_player.GetRoomPlayerState() == RoomPlayerState::WAIT;
		room_player.ResetGameData();
		if (!has_returned) session->SendPacket(reinterpret_cast<char*>(&return_p), return_p.header.size);
	}
}

void GameRoom::StoreRoomState(RoomState new_state)
{
	room_state_.store(new_state);
}

bool GameRoom::TryChangeRoomState(RoomState expected, RoomState desired)
{
	return room_state_.compare_exchange_strong(expected, desired);
}


// 슬롯을 먼저 비운 뒤 남은 플레이어에게 퇴장을 알린다. 퇴장자는 로비 복귀 시 로비가 별도로 알린다.
void GameRoom::CompletePlayerRemoval(SessionKey session_key, RoomExitType exit_type)
{
	auto* room_player = FindPlayer(session_key);
	if (!room_player) return;
	const int player_id = room_player->GetSessionKey().player_id;
	if (current_player_count_.load() == 1) BeginRoomDelete();
	if (exit_type == RoomExitType::KICK) {
		auto* session = server_->GetSessionManager().FindSession(session_key);
		if (session) {
			S2C_KICK_PACKET kick_p;
			kick_p.header = { static_cast<std::uint16_t>(sizeof(kick_p)), S2C_KICK };
			session->SendPacket(reinterpret_cast<char*>(&kick_p), kick_p.header.size);
		}
	}
	room_player->ClearPlayer();
	--current_player_count_;

	S2C_REMOVE_PLAYER_PACKET p;
	p.header.size = static_cast<std::uint16_t>(sizeof(p));
	p.header.type = S2C_REMOVE_PLAYER;
	p.player_id = player_id;
	Broadcast(reinterpret_cast<char*>(&p));

	if (player_id == host_id_) FindNewHost(); // 여기서 찾기 및 못찾을 경우 삭제까지 같이 함
}

void GameRoom::SendCreateRoom(Session& session)
{
	if (!IsPrivate()) {
		S2C_ADD_PUBLIC_ROOM_PACKET public_p;
		public_p.header.size = static_cast<std::uint16_t>(sizeof(public_p));
		public_p.header.type = S2C_ADD_PUBLIC_ROOM;
		memcpy(public_p.room_name, room_name_, sizeof(public_p.room_name));
		session.SendPacket(reinterpret_cast<char*>(&public_p), public_p.header.size);
	}
	else {
		S2C_ADD_PRIVATE_ROOM_PACKET private_p;
		private_p.header.size = static_cast<std::uint16_t>(sizeof(private_p));
		private_p.header.type = S2C_ADD_PRIVATE_ROOM;
		memcpy(private_p.room_name, room_name_, sizeof(private_p.room_name));
		session.SendPacket(reinterpret_cast<char*>(&private_p), private_p.header.size);
	}
	S2C_UPDATE_HOST_PACKET host_p{};
	host_p.header = {sizeof(host_p), S2C_UPDATE_HOST};
	host_p.new_host_id = host_id_;
	session.SendPacket(reinterpret_cast<const char*>(&host_p), sizeof(host_p));
}

// 대기·준비 중인 사람만 원하는 준비 상태로 바꾸고, 바뀐 뒤의 준비 상태를 방 전체에 알린다.
void GameRoom::SetPlayerReady(int player_id, bool is_ready)
{
	if (host_id_ == player_id) return;
	auto& room_players = room_players_;
	for (auto& room_player : room_players) {
		auto session = FindSession(room_player);
		if (!session) continue;
		if (session->GetSessionKey().player_id == player_id) { // 레디 상태
			const RoomPlayerState state = room_player.GetRoomPlayerState();
			if (state == RoomPlayerState::WAIT || state == RoomPlayerState::READY) room_player.SetRoomPlayerState(is_ready ? RoomPlayerState::READY : RoomPlayerState::WAIT);
			S2C_READY_PACKET p;
			p.header.size = static_cast<std::uint16_t>(sizeof(p));
			p.header.type = S2C_READY;
			p.player_id = player_id;
			p.is_ready = room_player.GetRoomPlayerState() == RoomPlayerState::READY;

			Broadcast(reinterpret_cast<char*>(&p));
			break;
		}
	}
}

void GameRoom::KickPlayer(int requester_id, int kick_player_id)
{
	if (requester_id != host_id_) return;
	if (kick_player_id == host_id_) return;
	auto& room_players = room_players_;

	for (auto& room_player : room_players) {
		if (!room_player.HasSession() || room_player.GetSessionKey().player_id != kick_player_id) continue;
		LeaveToLobby(room_player.GetSessionKey(), RoomExitType::KICK);
		return;
	}
}

void GameRoom::StartGame(int requester_id)
{
	if (requester_id != host_id_) return;
	if (room_state_.load() == RoomState::PLAY) return;
	auto& room_players = room_players_;

	int ready_player_count = 0;
	int result = -1;
	for (auto& room_player : room_players) {
		auto session = FindSession(room_player);
		if (!session) continue; // 사용 중이지 않은 인덱스는 건너뜀
		if (session->GetSessionKey().player_id == host_id_) ++ready_player_count;

		else if (room_player.GetRoomPlayerState() == RoomPlayerState::READY) ++ready_player_count;
	}

	int current_player_count = GetCurrentPlayerCount();
	if (ready_player_count == current_player_count) {
		if (current_player_count < MIN_START_PLAYER_COUNT) {
			result = ErrorCode::ROOM_NOT_ENOUGH_PLAYERS;
		}
		else {
			result = SUCCESS;
		}
	}
	else result = ErrorCode::ROOM_NOT_ALL_READY;

	if (result != SUCCESS) {
		S2C_ERROR_PACKET error_p;
		error_p.header.size = static_cast<std::uint16_t>(sizeof(error_p));
		error_p.header.type = S2C_ERROR;
		error_p.error_code = result;
		int host_index = FindHostIndex(host_id_);
		if (host_index >= 0) {
			auto host_session = FindSession(room_players[host_index]);
			if (host_session) host_session->SendPacket(reinterpret_cast<char*>(&error_p), error_p.header.size);
		}
		return;
	}

	if(!TryChangeRoomState(RoomState::WAIT, RoomState::PLAY)) return; // 상태 전환에 성공한 요청만 게임을 시작한다.

	S2C_MULTI_START_PACKET start_p;
	start_p.header.size = static_cast<std::uint16_t>(sizeof(start_p));
	start_p.header.type = S2C_MULTI_START;
	Broadcast(reinterpret_cast<char*>(&start_p));
	// 시작 조건을 충족한 플레이어를 로딩 상태로 전환하고, 로딩 완료를 기다린다.
	int player_count = 0;
	for (auto& room_player : room_players) {
		if (!FindSession(room_player)) continue;
		room_player.SetRoomPlayerState(RoomPlayerState::LOADING);
		room_player.ResetAmmo(server_->GetHitboxData().GetWeaponCount());
		room_player.ResetPickaxe();
		room_player.ResetHints();
		room_player.ResetItems();
		++player_count;
	}
	loading_deadline_ms_ = tick_time_ms_ + LOADING_TIMEOUT_MS;
	game_start_ms_ = 0;
	match_phase_ = MatchPhase::LOADING;
	last_pose_tick_ms_ = 0;
	// 보물을 놓고 그 위치로 힌트를 만든 뒤, 힌트 수에 맞춰 상자를 놓는다.
	PlaceTreasure();
	shot_seed_ = static_cast<std::uint64_t>(CreateRandomSeed()) << 32 | CreateRandomSeed();
	GenerateHints();
	PlaceBoxes(player_count);
	removed_cells_ = 0;
	next_area_removal_ms_ = 0;
	game_end_ms_ = 0;
}

// 최종 보물을 맵 경계에서 TREASURE_EDGE_MARGIN 안쪽의 무작위 위치에 놓는다. 보물을 놓지 않는 영역(건물·동양 유적 뒤 절벽·꽃밭) 밖이고, 보물 둘레 사각형 안의 땅이 모두 있고,
// 보물을 감싼 구가 배치물(건물·나무·바위)과 겹치지 않는 곳이 나올 때까지 다시 뽑는다.
// 바닥은 그 사각형 안 가장 낮은 땅에 맞춰 뜨지 않게 한다.
void GameRoom::PlaceTreasure()
{
	std::mt19937 random(CreateRandomSeed());
	std::uniform_real_distribution<float> random_x(MAP_MIN[0] + TREASURE_EDGE_MARGIN, MAP_MAX[0] - TREASURE_EDGE_MARGIN);
	std::uniform_real_distribution<float> random_y(MAP_MIN[1] + TREASURE_EDGE_MARGIN, MAP_MAX[1] - TREASURE_EDGE_MARGIN);
	std::uniform_real_distribution<float> random_yaw(0.0f, 360.0f);
	treasure_placement_ = {};
	for (int attempt = 0; attempt < TREASURE_PLACE_ATTEMPTS; ++attempt) {
		const float x = random_x(random);
		const float y = random_y(random);
		const std::optional<float> ground = FindTreasureGround(server_->GetMapData(), server_->GetMapRegionData(), x, y);
		if (!ground) continue;
		treasure_placement_ = { { x, y, *ground }, random_yaw(random) };
		return;
	}
	std::cerr << "Treasure place fail.. " << std::endl;
}

// 최종 보물 위치로 이번 게임의 힌트 목록을 만든다. 방향, 물체 종류마다 반경마다 개수 순서다.
// 거리는 보물 바닥에서 잰다.
void GameRoom::GenerateHints()
{
	const float x = treasure_placement_.location.x;
	const float y = treasure_placement_.location.y;
	const float z = treasure_placement_.location.z;
	hints_ = server_->GetDirectionHintData().JudgeDirections(x, y);
	for (std::uint8_t radius = 0; radius < std::size(HINT_OBJECT_COUNT_RADII); ++radius) {
		const HintKindCounts object_counts = server_->GetHintObjectTree().CountKinds(x, y, z, HINT_OBJECT_COUNT_RADII[radius]);
		for (std::size_t kind = 0; kind < object_counts.size(); ++kind) {
			hints_.push_back(CountHint{ static_cast<std::uint8_t>(kind), radius, static_cast<std::uint8_t>(std::min<std::uint32_t>(object_counts[kind], UINT8_MAX)) });
		}
	}
}

// 힌트마다 HINT_BOX_COPIES × 글로벌 배수만큼 힌트 상자를, 무기마다 AMMO_BOX_COUNT × 글로벌 배수만큼 탄약 상자를, 아이템 종류마다 등급별 ITEM_BOX_COUNTS × 글로벌 배수만큼 아이템 상자를,
// 블록마다 BLOCK_BOX_COUNT × 칸 수별 배수 × 글로벌 배수만큼 블록 상자를 만들어 섞고,
// 그 수만큼 상자 자리를 골라 상자마다 하나씩 넣는다.
// 글로벌 배수는 참가 인원 HINT_BOX_PLAYERS_PER_MULTIPLIER명마다 1씩 오른다. 배치 패킷은 상자 수에 맞춘 크기로 만든다.
void GameRoom::PlaceBoxes(int player_count)
{
	const int global_multiplier = std::max(1, (player_count + HINT_BOX_PLAYERS_PER_MULTIPLIER - 1) / HINT_BOX_PLAYERS_PER_MULTIPLIER);
	box_contents_.clear();
	for (std::uint32_t index = 0; index < hints_.size(); ++index) {
		box_contents_.insert(box_contents_.end(), HINT_BOX_COPIES * global_multiplier, BoxContent{ BoxContentKind::HINT, index });
	}
	for (std::uint32_t weapon = 0; weapon < std::size(AMMO_BOX_ROUNDS); ++weapon) {
		if (AMMO_BOX_ROUNDS[weapon] > 0) box_contents_.insert(box_contents_.end(), AMMO_BOX_COUNT * global_multiplier, BoxContent{ BoxContentKind::AMMO, weapon });
	}
	for (std::uint32_t kind = 0; kind < static_cast<std::uint32_t>(ItemKind::COUNT); ++kind) {
		for (std::uint8_t grade = 0; grade < std::size(ITEM_BOX_COUNTS); ++grade) {
			box_contents_.insert(box_contents_.end(), ITEM_BOX_COUNTS[grade] * global_multiplier, BoxContent{ BoxContentKind::ITEM, kind, grade });
		}
	}
	for (std::uint32_t piece = 0; piece < BLOCK_PIECE_COUNT; ++piece) {
		const std::size_t size_index = BLOCK_PIECES[piece].cell_count - 1;
		const int size_multiplier = size_index < std::size(BLOCK_BOX_SIZE_MULTIPLIERS) ? BLOCK_BOX_SIZE_MULTIPLIERS[size_index] : 1;
		box_contents_.insert(box_contents_.end(), BLOCK_BOX_COUNT * size_multiplier * global_multiplier, BoxContent{ BoxContentKind::BLOCK, piece });
	}
	std::mt19937 random(CreateRandomSeed());
	std::shuffle(box_contents_.begin(), box_contents_.end(), random);
	// 상자 번호는 16비트(상자 열기 패킷)라 그 범위까지만 만든다.
	box_contents_.resize(std::min<std::size_t>(box_contents_.size(), UINT16_MAX));
	const BoxCandidateData& box_candidates = server_->GetBoxCandidateData();
	box_placement_indices_ = box_candidates.SelectPlacements(box_contents_.size());
	// 상자 자리 후보가 모자라 덜 골랐으면 남는 내용물은 넣지 않는다.
	box_contents_.resize(box_placement_indices_.size());
	is_box_opened_.assign(box_placement_indices_.size(), false);
	box_layout_packet_ = BuildBoxLayoutPacket();
}

// 상자 배치 패킷들을 지금 열림 상태로 만든다. 상자 수에 맞춘 크기라 BUF_SIZE보다 클 수 있다.
std::vector<char> GameRoom::BuildBoxLayoutPacket() const
{
	// 패킷 하나에 담는 상자 수. 패킷 크기가 패킷 헤더의 크기 칸에 담을 수 있는 만큼을 넘지 않게 한다.
	constexpr std::size_t MAX_PACKET_BOX_COUNT = (UINT16_MAX - sizeof(S2C_BOX_LAYOUT_PACKET)) / sizeof(BOX_ENTRY);
	const BoxCandidateData& box_candidates = server_->GetBoxCandidateData();
	const std::size_t box_count = box_placement_indices_.size();
	const std::size_t packet_count = std::max<std::size_t>(1, (box_count + MAX_PACKET_BOX_COUNT - 1) / MAX_PACKET_BOX_COUNT);
	std::vector<char> packets(packet_count * sizeof(S2C_BOX_LAYOUT_PACKET) + box_count * sizeof(BOX_ENTRY));
	char* write = packets.data();
	for (std::size_t first = 0; first == 0 || first < box_count; first += MAX_PACKET_BOX_COUNT) {
		const std::size_t count = std::min(MAX_PACKET_BOX_COUNT, box_count - first);
		const std::size_t packet_size = sizeof(S2C_BOX_LAYOUT_PACKET) + count * sizeof(BOX_ENTRY);
		const S2C_BOX_LAYOUT_PACKET box_layout_p{ { static_cast<std::uint16_t>(packet_size), S2C_BOX_LAYOUT }, static_cast<std::uint16_t>(count) };
		std::memcpy(write, &box_layout_p, sizeof(box_layout_p));
		write += sizeof(box_layout_p);
		for (std::size_t index = first; index < first + count; ++index) {
			const BoxPlacement& placement = box_candidates.GetPlacements()[box_placement_indices_[index]];
			const BOX_ENTRY entry{ static_cast<std::uint8_t>(placement.kind), { placement.location.x, placement.location.y, placement.location.z }, { placement.rotation.x, placement.rotation.y, placement.rotation.z }, is_box_opened_[index] };
			std::memcpy(write, &entry, sizeof(entry));
			write += sizeof(entry);
		}
	}
	return packets;
}

void GameRoom::ProcessSpecificRoomTask(const RoomTask& task)
{
	const int requester_id = task.session_key.player_id;
	switch (task.task_type) {
	case RoomTaskType::READY:
		SetPlayerReady(requester_id, task.is_ready);
		break;
	case RoomTaskType::KICK:
		KickPlayer(requester_id, task.target_player_id);
		break;
	case RoomTaskType::START:
		StartGame(requester_id);
		break;
	default:
		break;
	}
}

// 중계할 이동 상태와 이번 틱에 받은 동작을 모아 받는 사람마다 한 번에 보낸다. 보낸 사람 것도 포함되며 클라이언트가 거른다.
void GameRoom::ProcessGameTick(long long tick_time_ms)
{
	ProcessLoadedPlayers();
	ProcessGameStart();
	ProcessAreaRemoval();
	ProcessStunRelease();
	ProcessReloadDone();
	ProcessTimeLimit();
	// 시상식 중에는 게임 진행을 멈추고, 끝날 시각이 되면 방으로 돌려보낸다.
	if (award_end_ms_ != 0) {
		if (tick_time_ms >= award_end_ms_) ClearGame();
		return;
	}
	if (game_end_ms_ != 0 && tick_time_ms >= game_end_ms_) {
		StartAward();
		return;
	}
	// 플레이 중인 플레이어의 판정용 자세를 지난 틱 이후 시간만큼 진행한다.
	const float pose_delta_seconds = last_pose_tick_ms_ != 0 ? (tick_time_ms - last_pose_tick_ms_) / 1000.0f : 0.0f;
	last_pose_tick_ms_ = tick_time_ms;
	for (auto& room_player : room_players_) {
		if (!room_player.HasSession() || room_player.GetRoomPlayerState() != RoomPlayerState::PLAY || !room_player.HasMoveState()) continue;
		room_player.GetPose().Update(room_player.GetMoveState(), pose_delta_seconds, tick_time_ms);
	}
	// 공중에서 이동 상태가 MOVE_AIR_DELAY_SECONDS 넘게 오지 않으면 서버가 계산한 공중 위치로 맞춘다.
	for (auto& room_player : room_players_) {
		if (room_player.GetRoomPlayerState() != RoomPlayerState::PLAY || !room_player.HasMoveState() || room_player.IsAwaitingMoveCorrection()) continue;
		if (room_player.GetAirState(tick_time_ms).phase == AirPhase::GROUND || tick_time_ms - room_player.GetMoveTimeMs() <= static_cast<long long>(MOVE_AIR_DELAY_SECONDS * 1000.0f)) continue;
		if (auto session = FindSession(room_player)) CorrectMove(room_player, *session, true);
	}
	char send_buffer[BUF_SIZE];
	int send_data_size = 0;
	const auto flush = [&]() {
		if (send_data_size == 0) return;
		for (auto& room_player : room_players_) {
			auto session = FindSession(room_player);
			if (!session || room_player.GetRoomPlayerState() != RoomPlayerState::PLAY) continue;
			session->SendPacket(send_buffer, send_data_size);
		}
		send_data_size = 0;
	};
	const auto append = [&](const void* packet, int packet_size) {
		if (send_data_size + packet_size > BUF_SIZE) flush();
		memcpy(send_buffer + send_data_size, packet, packet_size);
		send_data_size += packet_size;
	};

	for (auto& room_player : room_players_) {
		if (!room_player.HasSession() || !room_player.CanRelayMoveState()) continue;
		const MOVE_STATE& state = room_player.GetMoveState();
		S2C_MOVE_PACKET move_p;
		move_p.header = { static_cast<std::uint16_t>(sizeof(move_p)), S2C_MOVE };
		move_p.player_id = room_player.GetSessionKey().player_id;
		std::memcpy(move_p.location, state.location, sizeof(move_p.location));
		std::memcpy(move_p.velocity, state.velocity, sizeof(move_p.velocity));
		move_p.yaw = state.yaw;
		move_p.view_pitch = state.view_pitch;
		move_p.air_phase = state.air_phase;
		move_p.client_time_ms = state.client_time_ms;
		append(&move_p, sizeof(move_p));
		room_player.MarkMoveStateRelayed();
	}
	for (std::size_t offset = 0; offset < pending_relays_.size();) {
		const int packet_size = reinterpret_cast<const PACKET_HEADER*>(pending_relays_.data() + offset)->size;
		append(pending_relays_.data() + offset, packet_size);
		offset += packet_size;
	}
	pending_relays_.clear();
	flush();
}

void GameRoom::QueueRelay(const void* packet, int packet_size)
{
	const char* bytes = static_cast<const char*>(packet);
	pending_relays_.insert(pending_relays_.end(), bytes, bytes + packet_size);
}

int GameRoom::AppendInputState(char* buffer, const Player& player) const
{
	const int player_id = player.GetSessionKey().player_id;
	const S2C_MOVE_INPUT_PACKET input_p{ { static_cast<std::uint16_t>(sizeof(S2C_MOVE_INPUT_PACKET)), S2C_MOVE_INPUT }, player_id, player.GetInputRight(), player.GetInputForward() };
	const S2C_AIM_PACKET aim_p{ { static_cast<std::uint16_t>(sizeof(S2C_AIM_PACKET)), S2C_AIM }, player_id, player.IsAiming() };
	const S2C_EQUIP_PACKET equip_p{ { static_cast<std::uint16_t>(sizeof(S2C_EQUIP_PACKET)), S2C_EQUIP }, player_id, player.GetWeapon() };
	std::memcpy(buffer, &input_p, sizeof(input_p));
	std::memcpy(buffer + sizeof(input_p), &aim_p, sizeof(aim_p));
	std::memcpy(buffer + sizeof(input_p) + sizeof(aim_p), &equip_p, sizeof(equip_p));
	return sizeof(input_p) + sizeof(aim_p) + sizeof(equip_p);
}

// 로딩을 마친 플레이어를 마치는 대로 스폰하고 맵을 보여 준다.
// 이미 시작 대기 중이면 남은 시간을, 게임이 시작됐으면 게임 시작을 이어서 보낸다. 게임 도중 합류하면 아직 줍지 않은 떨어진 힌트와 공개된 힌트도 보낸다.
void GameRoom::ProcessLoadedPlayers()
{
	bool has_loaded_player = false;
	for (auto& room_player : room_players_) {
		if (!FindSession(room_player)) continue;
		has_loaded_player |= room_player.GetRoomPlayerState() == RoomPlayerState::LOADED;
	}
	if (!has_loaded_player) return;

	// 합류하는 플레이어의 시작 좌표는 방 슬롯 번호로 정하고, 이미 플레이 중인 플레이어는 지금 자리를 보낸다.
	// 플레이어마다 스폰 뒤에 이동 키 입력·조준·장비를 이어 붙여, 이번에 합류하는 플레이어만 담은 것과 이미 플레이 중인 플레이어까지 담은 것을 만든다.
	constexpr int SPAWN_STATE_SIZE = sizeof(S2C_SPAWN_PACKET) + sizeof(S2C_MOVE_INPUT_PACKET) + sizeof(S2C_AIM_PACKET) + sizeof(S2C_EQUIP_PACKET);
	char joined_buffer[MAX_ROOM_PLAYER_COUNT * SPAWN_STATE_SIZE];
	char all_buffer[MAX_ROOM_PLAYER_COUNT * SPAWN_STATE_SIZE];
	int joined_size = 0;
	int all_size = 0;
	for (int slot = 0; slot < MAX_ROOM_PLAYER_COUNT; ++slot) {
		auto& room_player = room_players_[slot];
		if (!FindSession(room_player)) continue;
		const RoomPlayerState state = room_player.GetRoomPlayerState();
		if (state != RoomPlayerState::LOADED && state != RoomPlayerState::PLAY) continue;
		S2C_SPAWN_PACKET spawn_p;
		spawn_p.header = { static_cast<std::uint16_t>(sizeof(spawn_p)), S2C_SPAWN };
		spawn_p.player_id = room_player.GetSessionKey().player_id;
		if (state == RoomPlayerState::LOADED) {
			const SpawnPoint& point = START_SPAWN_POINTS[slot];
			spawn_p.location[0] = point.x;
			spawn_p.location[1] = point.y;
			spawn_p.location[2] = point.z;
			spawn_p.yaw = point.yaw;
			room_player.SetSpawnLocation(spawn_p.location, spawn_p.yaw, tick_time_ms_);
		}
		else {
			std::memcpy(spawn_p.location, room_player.GetMoveState().location, sizeof(spawn_p.location));
			spawn_p.yaw = room_player.GetMoveState().yaw;
		}
		char* entry = all_buffer + all_size;
		std::memcpy(entry, &spawn_p, sizeof(spawn_p));
		const int entry_size = sizeof(spawn_p) + AppendInputState(entry + sizeof(spawn_p), room_player);
		all_size += entry_size;
		if (state == RoomPlayerState::LOADED) {
			std::memcpy(joined_buffer + joined_size, entry, entry_size);
			joined_size += entry_size;
		}
	}
	S2C_PLAY_START_PACKET start_p;
	start_p.header = { static_cast<std::uint16_t>(sizeof(start_p)), S2C_PLAY_START };
	S2C_GAME_READY_PACKET ready_p;
	ready_p.header = { static_cast<std::uint16_t>(sizeof(ready_p)), S2C_GAME_READY };
	ready_p.remaining_ms = static_cast<std::uint32_t>(std::max(0LL, game_start_ms_ - tick_time_ms_));
	S2C_GAME_START_PACKET game_start_p;
	game_start_p.header = { static_cast<std::uint16_t>(sizeof(game_start_p)), S2C_GAME_START };
	game_start_p.remaining_ms = static_cast<std::uint32_t>(std::max(0LL, game_start_ms_ + GAME_TIME_LIMIT_MS - tick_time_ms_));

	// 합류하는 플레이어는 상자 배치, 전원의 스폰, 맵 표시를, 이미 플레이 중인 플레이어는 합류하는 플레이어의 스폰만 받는다.
	for (auto& room_player : room_players_) {
		auto session = FindSession(room_player);
		if (!session) continue;
		if (room_player.GetRoomPlayerState() == RoomPlayerState::LOADED) {
			room_player.GetPose().Reset(server_->GetHitboxData());
			S2C_SHOT_SEED_PACKET seed_p;
			seed_p.header = { static_cast<std::uint16_t>(sizeof(seed_p)), S2C_SHOT_SEED };
			seed_p.seed = shot_seed_;
			session->SendPacket(reinterpret_cast<char*>(&seed_p), seed_p.header.size);
			if (match_phase_ == MatchPhase::STARTED) {
				const std::vector<char> box_layout_packet = BuildBoxLayoutPacket();
				session->SendLargePacket(box_layout_packet.data(), static_cast<int>(box_layout_packet.size()));
			}
			else {
				session->SendLargePacket(box_layout_packet_.data(), static_cast<int>(box_layout_packet_.size()));
			}
			session->SendPacket(all_buffer, all_size);
			session->SendPacket(reinterpret_cast<char*>(&start_p), start_p.header.size);
			if (match_phase_ == MatchPhase::STARTED) {
				session->SendPacket(reinterpret_cast<char*>(&game_start_p), game_start_p.header.size);
				// 지금까지 지운 칸이 든 배제 판과 다음 지우기까지 남은 시간을 받는다.
				char board_buffer[sizeof(S2C_EXCLUSION_BOARD_PACKET)];
				const int board_size = AppendExclusionBoard(board_buffer, room_player);
				if (board_size > 0) session->SendPacket(board_buffer, board_size);
				SendAreaRemovalTime(*session);
			}
			else if (match_phase_ == MatchPhase::READY) session->SendPacket(reinterpret_cast<char*>(&ready_p), ready_p.header.size);
			room_player.SetRoomPlayerState(RoomPlayerState::PLAY);
		}
		else if (room_player.GetRoomPlayerState() == RoomPlayerState::PLAY) {
			session->SendPacket(joined_buffer, joined_size);
		}
	}
}

// 클라이언트가 보낸 명중을 검증하고, 인정하면 쏜 사람과 맞은 사람에게만 알린 뒤 맞은 사람의 기절 게이지를 올린다. 거부하면 아무것도 보내지 않는다.
// 총 발사와 재장전은 서버 탄약 상태로, 곡괭이 휘두르기는 사용 가능 횟수로 확인해 인정한다.
// 곡괭이를 인정하지 않으면 사용 가능 횟수를 보내 클라이언트를 맞추고, 총 발사를 인정하면 남은 탄창 탄을 쏜 사람에게 보낸다. 그 밖의 동작은 그대로 인정한다.
bool GameRoom::AcceptAction(Player& player, Session& session, PlayerAction action, std::uint8_t weapon_value)
{
	if (action == PlayerAction::MELEE && weapon_value == static_cast<std::uint8_t>(WeaponType::PICKAXE)) {
		if (player.TryUsePickaxe(tick_time_ms_)) {
			DigTreasure(player);
			return true;
		}
		S2C_PICKAXE_CHARGES_PACKET charges_p;
		charges_p.header = { static_cast<std::uint16_t>(sizeof(charges_p)), S2C_PICKAXE_CHARGES };
		charges_p.charges = static_cast<std::uint8_t>(player.GetPickaxeCharges());
		charges_p.remaining_ms = static_cast<std::uint32_t>(player.GetPickaxeChargeRemainingMs(tick_time_ms_));
		session.SendPacket(reinterpret_cast<char*>(&charges_p), charges_p.header.size);
		return false;
	}
	const WeaponInfo* weapon = server_->GetHitboxData().GetWeaponInfo(weapon_value);
	if (!weapon || !weapon->is_hitscan || (action != PlayerAction::FIRE && action != PlayerAction::RELOAD)) return true;
	if (action == PlayerAction::RELOAD) return player.TryStartReload(weapon_value, *weapon, tick_time_ms_);
	if (!player.TryFire(weapon_value, *weapon, tick_time_ms_)) return false;
	S2C_AMMO_USE_PACKET use_p;
	use_p.header = { static_cast<std::uint16_t>(sizeof(use_p)), S2C_AMMO_USE };
	use_p.weapon = weapon_value;
	use_p.rounds = static_cast<std::uint16_t>(player.GetAmmo(weapon_value)->rounds);
	session.SendPacket(reinterpret_cast<char*>(&use_p), use_p.header.size);
	return true;
}

// 명중은 서버가 인정한 발사 하나를 써서 판정한다.
void GameRoom::ProcessHit(Player& shooter, const C2S_HIT_PACKET& request_p)
{
	if (!shooter.ClaimShot(request_p.weapon)) return;
	Player* target = nullptr;
	for (auto& room_player : room_players_) {
		if (room_player.HasSession() && room_player.GetSessionKey().player_id == request_p.target_player_id) target = &room_player;
	}
	if (!target || target == &shooter || target->GetRoomPlayerState() != RoomPlayerState::PLAY || !CheckHit(shooter, *target, request_p)) return;

	S2C_HIT_PACKET hit_p;
	hit_p.header = { static_cast<std::uint16_t>(sizeof(hit_p)), S2C_HIT };
	hit_p.shooter_player_id = shooter.GetSessionKey().player_id;
	if (auto session = FindSession(*target)) session->SendPacket(reinterpret_cast<char*>(&hit_p), hit_p.header.size);
	AddStunGauge(shooter, *target, request_p);
}

// 쏜 사람 이동 상태의 위치와 몸 방향으로 탄 광선 시작점(캡슐 안 어깨 고정점)을 구한다. 클라이언트도 같은 식으로 구한다.
static DirectX::XMFLOAT3 GetShotOrigin(const MOVE_STATE& move_state)
{
	const float yaw = DirectX::XMConvertToRadians(move_state.yaw);
	const float cos_yaw = std::cos(yaw);
	const float sin_yaw = std::sin(yaw);
	return { move_state.location[0] + SHOT_ORIGIN_FORWARD * cos_yaw - SHOT_ORIGIN_RIGHT * sin_yaw, move_state.location[1] + SHOT_ORIGIN_FORWARD * sin_yaw + SHOT_ORIGIN_RIGHT * cos_yaw, move_state.location[2] + SHOT_ORIGIN_UP };
}

// 무기별 증가량에 맞은 부위 배율을 곱해 게이지를 올린다. 기절 중이거나 기절이 끝난 뒤 보호 중이면 게이지는 그대로다.
// 최대치에 닿으면 기절을 방 전체에 알린 뒤, 기절한 사람의 힌트 하나를 쏜 사람에게 넘기고 쏜 사람의 기절왕 기록을 늘린다. 아니면 바뀐 값을 맞은 사람에게만 보낸다.
void GameRoom::AddStunGauge(Player& shooter, Player& target, const C2S_HIT_PACKET& request_p)
{
	if (request_p.weapon >= std::size(STUN_GAUGE_WEAPON_GAINS) || request_p.part >= std::size(STUN_GAUGE_PART_MULTIPLIERS) || target.IsStunProtected(tick_time_ms_)) return;
	if (target.AddStunGauge(STUN_GAUGE_WEAPON_GAINS[request_p.weapon] * STUN_GAUGE_PART_MULTIPLIERS[request_p.part], tick_time_ms_)) {
		// 기절하면 조작을 받지 않으므로 이동 키 입력과 조준을 지운다. 클라이언트도 기절 적용을 받으면 스스로 지운다. 이동은 중력과 관성대로 이어진다.
		target.SetMoveInput(0, 0);
		target.SetAiming(false, tick_time_ms_);
		S2C_STUN_APPLY_PACKET stun_p;
		stun_p.header = { static_cast<std::uint16_t>(sizeof(stun_p)), S2C_STUN_APPLY };
		stun_p.player_id = request_p.target_player_id;
		// 기절한 사람의 힌트 하나를 무작위로 골라 쏜 사람에게 넘긴다. 쏜 사람에게는 새로 얻은 힌트를, 기절한 사람에게는 잃은 힌트 번호를 기절 적용 뒤에 붙인다.
		char shooter_buffer[sizeof(stun_p) + MAX_HINT_PACKET_SIZE];
		char target_buffer[sizeof(stun_p) + sizeof(S2C_HINT_LOST_PACKET)];
		std::memcpy(shooter_buffer, &stun_p, sizeof(stun_p));
		std::memcpy(target_buffer, &stun_p, sizeof(stun_p));
		int shooter_size = sizeof(stun_p);
		int target_size = sizeof(stun_p);
		if (const std::optional<std::uint32_t> hint = target.TakeRandomHint()) {
			if (shooter.AddHint(*hint)) shooter_size += AppendHintPacket(shooter_buffer + shooter_size, *hint);
			S2C_HINT_LOST_PACKET lost_p;
			lost_p.header = { static_cast<std::uint16_t>(sizeof(lost_p)), S2C_HINT_LOST };
			lost_p.hint_id = static_cast<std::uint16_t>(*hint);
			std::memcpy(target_buffer + target_size, &lost_p, sizeof(lost_p));
			target_size += sizeof(lost_p);
		}
		for (auto& room_player : room_players_) {
			auto session = FindSession(room_player);
			if (!session || room_player.GetRoomPlayerState() != RoomPlayerState::PLAY) continue;
			if (&room_player == &shooter) session->SendPacket(shooter_buffer, shooter_size);
			else if (&room_player == &target) session->SendPacket(target_buffer, target_size);
			else session->SendPacket(reinterpret_cast<char*>(&stun_p), stun_p.header.size);
		}
		shooter.AddAwardCount(AwardCategory::STUN, tick_time_ms_);
		return;
	}
	S2C_STUN_GAUGE_PACKET gauge_p;
	gauge_p.header = { static_cast<std::uint16_t>(sizeof(gauge_p)), S2C_STUN_GAUGE };
	gauge_p.gauge = target.GetStunGauge(tick_time_ms_);
	if (auto session = FindSession(target)) session->SendPacket(reinterpret_cast<char*>(&gauge_p), gauge_p.header.size);
}

// point가 location이 중심인 캐릭터 캡슐을 margin(cm)만큼 키운 안에 있는지 본다.
static bool IsInsideCharacterCapsule(const DirectX::XMFLOAT3& point, const float location[3], float margin)
{
	const float half_segment = CHARACTER_CAPSULE_HALF_HEIGHT - CHARACTER_CAPSULE_RADIUS;
	const float height = point.z - location[2];
	const float axis_height = std::clamp(height, -half_segment, half_segment);
	return std::hypot(point.x - location[0], point.y - location[1], height - axis_height) <= CHARACTER_CAPSULE_RADIUS + margin;
}

// 지연 보정이 없으므로 클라이언트 화면과 서버 위치 차이를 여유로 허용한다.
// 총인지, 사거리 안인지, 탄 퍼짐이 맞는지 본 뒤, 서버가 구한 시작점에서 맞은 사람의 서버 히트박스를 속도에 비례한 여유만큼 키워 광선이 보낸 부위를 지나가는지 검사한다.
// 마지막으로 시작점부터 히트박스에 닿은 곳까지 랜드스케이프나 총알을 막는 배치물(건물·나무·바위)이 가로막지 않는지, 맞은 곳이 쏜 사람 카메라에서 보였는지 본다.
bool GameRoom::CheckHit(const Player& shooter, const Player& target, const C2S_HIT_PACKET& request_p) const
{
	const HitboxData& data = server_->GetHitboxData();
	const WeaponInfo* weapon = data.GetWeaponInfo(request_p.weapon);
	if (!data.IsLoaded() || !weapon || !weapon->is_hitscan || request_p.part >= static_cast<std::uint8_t>(HitboxPart::COUNT)) return false;
	if (!shooter.HasMoveState() || !target.HasMoveState()) return false;
	if (!std::isfinite(request_p.distance) || request_p.distance < 0.0f || request_p.distance > weapon->hitscan_distance) return false;

	const DirectX::XMVECTOR direction = DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(request_p.direction));
	const float direction_length = DirectX::XMVectorGetX(DirectX::XMVector3Length(direction));
	if (!std::isfinite(direction_length) || direction_length < 0.5f) return false;
	DirectX::XMFLOAT3 unit_direction;
	DirectX::XMStoreFloat3(&unit_direction, DirectX::XMVectorScale(direction, 1.0f / direction_length));
	if (!CheckShotSpread(shooter, request_p, unit_direction)) return false;
	const DirectX::XMFLOAT3 origin = GetShotOrigin(shooter.GetMoveState());

	const MOVE_STATE& target_state = target.GetMoveState();
	const float target_speed = std::hypot(target_state.velocity[0], target_state.velocity[1], target_state.velocity[2]);
	const float margin = HIT_BASE_MARGIN + target_speed * HIT_LATENCY_ALLOWANCE_SECONDS;
	const float max_distance = request_p.distance + margin;
	const DirectX::XMFLOAT3 location(target_state.location[0], target_state.location[1], target_state.location[2]);
	if (!IsRayNearCharacter(origin, unit_direction, max_distance, location, target_state.yaw, data.GetBroadphaseCapsule(), margin)) return false;
	// 클라이언트가 보낸 부위의 도형만 검사해, 광선이 그 부위를 지나지 않으면 인정하지 않는다.
	std::vector<HitboxShape> shapes = target.GetPose().BuildWorldHitboxes(target_state, tick_time_ms_);
	std::erase_if(shapes, [&](const HitboxShape& shape) { return shape.part != static_cast<HitboxPart>(request_p.part); });
	const HitboxRayHit hit = RaycastHitboxes(origin, unit_direction, max_distance, shapes, margin);
	if (!hit.has_hit) return false;
	DirectX::XMFLOAT3 hit_point;
	DirectX::XMStoreFloat3(&hit_point, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&origin), DirectX::XMVectorScale(DirectX::XMLoadFloat3(&unit_direction), hit.distance)));
	return !IsShotPathBlocked(origin, hit_point) && CheckShotCamera(shooter, DirectX::XMFLOAT3(request_p.camera[0], request_p.camera[1], request_p.camera[2]), hit_point);
}

// 받은 카메라 위치가 쏜 사람 카메라가 있을 수 있는 자리인지, 그 카메라에서 맞은 곳이 보였는지 본다.
// 조준 카메라는 캡슐 기준 고정 위치에서 위아래 흔들림 안이어야 한다.
// 3인칭 카메라는 팔 끝(땅 가까이면 팔을 올린 끝)이어야 하고, 그보다 당겨졌으면 카메라 충돌 구가 지형·배치물·다른 플레이어에 닿아 있으면서 팔 시작점에서 카메라까지는 막히지 않아야 한다(팔을 따라가다 처음 닿은 곳).
// 3인칭 카메라 방향은 카메라 위치에서 팔 모양으로 거꾸로 구하고, 맞은 곳이 그 방향에서 CAMERA_VIEW_CONE_DEGREES 안이어야 한다. 마지막으로 카메라에서 맞은 곳까지 막히지 않아야 한다.
bool GameRoom::CheckShotCamera(const Player& shooter, const DirectX::XMFLOAT3& camera, const DirectX::XMFLOAT3& hit_point) const
{
	if (!std::isfinite(camera.x) || !std::isfinite(camera.y) || !std::isfinite(camera.z)) return false;
	const MOVE_STATE& state = shooter.GetMoveState();
	const float yaw = DirectX::XMConvertToRadians(state.yaw);
	const float tolerance = CAMERA_POSITION_TOLERANCE + std::hypot(state.velocity[0], state.velocity[1], state.velocity[2]) * CAMERA_FRAME_ALLOWANCE_SECONDS;
	if (shooter.IsAiming()) {
		const float horizontal = std::hypot(camera.x - (state.location[0] + AIM_CAMERA_FORWARD * std::cos(yaw)), camera.y - (state.location[1] + AIM_CAMERA_FORWARD * std::sin(yaw)));
		if (horizontal > tolerance || std::abs(camera.z - (state.location[2] + AIM_CAMERA_UP)) > tolerance + AIM_CAMERA_BOB_RANGE) return false;
		return !IsShotPathBlocked(camera, hit_point);
	}

	const LandscapeData& landscape = server_->GetMapData().GetLandscape();
	const CollisionObjectData& collision_objects = server_->GetMapData().GetCollisionObjects();
	const float arm_length = std::hypot(CAMERA_ARM_LENGTH, CAMERA_SOCKET_RIGHT, CAMERA_SOCKET_UP);
	// 팔 시작점 높이가 pivot_z일 때 카메라가 팔 위에 있을 수 있는 자리이고 맞은 곳이 화면 안이었는지 본다.
	const auto is_arm_camera = [&](float pivot_z)
	{
		const DirectX::XMFLOAT3 pivot(state.location[0], state.location[1], pivot_z);
		const float offset[3] = { camera.x - pivot.x, camera.y - pivot.y, camera.z - pivot.z };
		const float distance = std::hypot(offset[0], offset[1], offset[2]);
		if (distance > arm_length + tolerance) return false;
		if (distance < arm_length - tolerance) {
			const float probe = CAMERA_PROBE_RADIUS + tolerance;
			DirectX::XMFLOAT3 probe_end;
			DirectX::XMStoreFloat3(&probe_end, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&camera), DirectX::XMVectorScale(DirectX::XMVectorSet(offset[0], offset[1], offset[2], 0.0f), probe / std::max(distance, 1.0f))));
			const std::optional<float> ground = landscape.GetGroundHeight(camera.x, camera.y);
			bool is_touching = (ground && camera.z - probe <= *ground) || landscape.IsSegmentBlockedByLandscape(camera, probe_end)
				|| collision_objects.IsCapsuleOverlappingObjects(camera, probe, probe, TERRAIN_BLOCKS_VISIBILITY | TERRAIN_BLOCKS_PAWN);
			for (const auto& room_player : room_players_) {
				if (&room_player == &shooter || room_player.GetRoomPlayerState() != RoomPlayerState::PLAY || !room_player.HasMoveState()) continue;
				const MOVE_STATE& other = room_player.GetMoveState();
				is_touching = is_touching || IsInsideCharacterCapsule(camera, other.location, probe + std::hypot(other.velocity[0], other.velocity[1], other.velocity[2]) * HIT_LATENCY_ALLOWANCE_SECONDS);
			}
			if (!is_touching || landscape.IsSegmentBlockedByLandscape(pivot, camera) || collision_objects.IsSegmentBlockedByObjects(pivot, camera, TERRAIN_BLOCKS_VISIBILITY | TERRAIN_BLOCKS_PAWN)) return false;
		}
		// 팔은 시선 반대 방향 팔 길이와 시선 기준 오른쪽·위 오프셋을 더한 모양이다. 높이로 상하 각도를, 수평 방향으로 좌우 각도를 구한다.
		// 팔 시작점 가까이 당겨졌으면 방향을 구할 수 없으므로 보지 않는다.
		if (distance < arm_length * 0.2f) return true;
		const float arm_up = offset[2] * arm_length / distance;
		const float pitch = std::acos(std::clamp(arm_up / std::hypot(CAMERA_ARM_LENGTH, CAMERA_SOCKET_UP), -1.0f, 1.0f)) - std::atan2(CAMERA_ARM_LENGTH, CAMERA_SOCKET_UP);
		const float arm_back = -CAMERA_ARM_LENGTH * std::cos(pitch) - CAMERA_SOCKET_UP * std::sin(pitch);
		const float arm_yaw = std::atan2(offset[1], offset[0]) - std::atan2(CAMERA_SOCKET_RIGHT, arm_back);
		const float view_pitch = pitch - DirectX::XMConvertToRadians(CAMERA_PITCH_DOWN_DEGREES);
		const DirectX::XMVECTOR forward = DirectX::XMVectorSet(std::cos(view_pitch) * std::cos(arm_yaw), std::cos(view_pitch) * std::sin(arm_yaw), std::sin(view_pitch), 0.0f);
		const DirectX::XMVECTOR to_hit = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&hit_point), DirectX::XMLoadFloat3(&camera)));
		return DirectX::XMVectorGetX(DirectX::XMVector3Dot(forward, to_hit)) >= std::cos(DirectX::XMConvertToRadians(CAMERA_VIEW_CONE_DEGREES));
	};
	if (is_arm_camera(state.location[2])) return !IsShotPathBlocked(camera, hit_point);
	// 올려다보면 팔 끝이 땅 가까이 내려가 팔 전체를 올리고, 카메라는 땅에서 CAMERA_GROUND_CLEARANCE 높이에 온다.
	// 올리지 않은 팔로 맞지 않으면 팔 끝이 카메라에 오도록 올린 높이를 구하고, 카메라 바로 아래 그 높이 안에 땅이나 배치물이 있을 때만 올린 팔로 다시 본다.
	const float horizontal = std::hypot(camera.x - state.location[0], camera.y - state.location[1]);
	if (horizontal > arm_length) return false;
	const float raise = camera.z - state.location[2] + std::sqrt(arm_length * arm_length - horizontal * horizontal);
	const DirectX::XMFLOAT3 below(camera.x, camera.y, camera.z - CAMERA_GROUND_CLEARANCE - tolerance);
	if (raise <= 0.0f || (!landscape.IsSegmentBlockedByLandscape(camera, below) && !collision_objects.IsSegmentBlockedByObjects(camera, below, TERRAIN_BLOCKS_VISIBILITY | TERRAIN_BLOCKS_PAWN))) return false;
	return is_arm_camera(state.location[2] + raise) && !IsShotPathBlocked(camera, hit_point);
}

// start에서 end까지 랜드스케이프나 총알을 막는 배치물이 가로막는지 본다.
// 벽에 붙은 시작점이나 벽에 맞은 탄 끝을 막힘으로 보지 않게 양 끝을 HIT_OCCLUSION_MARGIN만큼 줄인다.
bool GameRoom::IsShotPathBlocked(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end) const
{
	const DirectX::XMVECTOR start_v = DirectX::XMLoadFloat3(&start);
	const DirectX::XMVECTOR end_v = DirectX::XMLoadFloat3(&end);
	const float length = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(end_v, start_v)));
	if (!std::isfinite(length) || length <= HIT_OCCLUSION_MARGIN * 2.0f) return false;
	DirectX::XMFLOAT3 segment_start;
	DirectX::XMFLOAT3 segment_end;
	DirectX::XMStoreFloat3(&segment_start, DirectX::XMVectorLerp(start_v, end_v, HIT_OCCLUSION_MARGIN / length));
	DirectX::XMStoreFloat3(&segment_end, DirectX::XMVectorLerp(start_v, end_v, 1.0f - HIT_OCCLUSION_MARGIN / length));
	return server_->GetMapData().GetLandscape().IsSegmentBlockedByLandscape(segment_start, segment_end)
		|| server_->GetMapData().GetCollisionObjects().IsSegmentBlockedByObjects(segment_start, segment_end, TERRAIN_BLOCKS_VISIBILITY);
}

// 명중에 쓴 발사의 탄 퍼짐을 확인한다. 쓴 퍼짐 크기는 그 발의 최소치와 무기 최대 사이여야 한다.
// 조준 방향에 서버가 (시드, 아이디, 무기, 발사 번호)로 다시 계산한 빗나감을 적용한 방향이 받은 광선 방향과 같아야 한다.
bool GameRoom::CheckShotSpread(const Player& shooter, const C2S_HIT_PACKET& request_p, const DirectX::XMFLOAT3& direction) const
{
	const WeaponAmmo* ammo = shooter.GetAmmo(request_p.weapon);
	const ShotSpreadSettings& spread = GetShotSpreadSettings(request_p.weapon);
	if (!ammo || !std::isfinite(request_p.spread) || request_p.spread < ammo->last_shot_min_spread - SHOT_SPREAD_EPSILON_DEGREES || request_p.spread > spread.max + SHOT_SPREAD_EPSILON_DEGREES) return false;
	const DirectX::XMVECTOR aim = DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(request_p.aim_direction));
	const float aim_length = DirectX::XMVectorGetX(DirectX::XMVector3Length(aim));
	if (!std::isfinite(aim_length) || aim_length < 0.5f) return false;
	DirectX::XMFLOAT3 unit_aim;
	DirectX::XMStoreFloat3(&unit_aim, DirectX::XMVectorScale(aim, 1.0f / aim_length));
	const std::uint64_t hash = GetShotSpreadHash(shot_seed_, shooter.GetSessionKey().player_id, request_p.weapon, ammo->last_shot_index);
	const ShotSpreadDirection expected = ApplyShotSpread(unit_aim.x, unit_aim.y, unit_aim.z, request_p.spread, hash);
	const float shot_dot = expected.x * direction.x + expected.y * direction.y + expected.z * direction.z;
	return shot_dot >= std::cos(DirectX::XMConvertToRadians(SHOT_SPREAD_DIRECTION_TOLERANCE_DEGREES));
}

// 게임이 시작된 뒤 아직 열리지 않은 상자만 연다. 열면 플레이 중인 방 전체에 상자 열림을 보내고, 연 사람에게는 상자 열림 뒤에 내용물을 이어 보낸다.
// 힌트 상자는 그 힌트를 가진 것으로 기록하고(이미 가졌으면 그대로) 보내며, 탄약 상자는 소지 탄을 늘린 뒤 그 무기의 탄약 상태를 보낸다.
// 아이템 상자는 그 아이템을, 블록 상자는 그 블록을 가진 것으로 기록하고 보낸다.
void GameRoom::ProcessBoxOpen(Player& opener, std::uint16_t box, std::optional<std::uint8_t> shot_weapon)
{
	// 쏴서 열면 서버가 인정한 발사 하나를 쓴다.
	if (shot_weapon && !opener.ClaimShot(*shot_weapon)) return;
	if (match_phase_ != MatchPhase::STARTED || box >= is_box_opened_.size() || is_box_opened_[box] || !CheckBoxOpen(opener, box, shot_weapon)) return;
	is_box_opened_[box] = true;
	opener.AddAwardCount(AwardCategory::BOX_OPEN, tick_time_ms_);

	S2C_BOX_OPEN_PACKET open_p;
	open_p.header = { static_cast<std::uint16_t>(sizeof(open_p)), S2C_BOX_OPEN };
	open_p.box = box;
	char opener_buffer[sizeof(open_p) + std::max({ MAX_HINT_PACKET_SIZE, sizeof(S2C_AMMO_GAIN_PACKET), sizeof(S2C_ITEM_PACKET), sizeof(S2C_BLOCK_PACKET) })];
	std::memcpy(opener_buffer, &open_p, sizeof(open_p));
	int opener_size = sizeof(open_p);
	const BoxContent& content = box_contents_[box];
	if (content.kind == BoxContentKind::HINT) {
		opener.AddHint(content.value);
		opener_size += AppendHintPacket(opener_buffer + opener_size, content.value);
	}
	else if (content.kind == BoxContentKind::BLOCK) {
		opener.AddBlock(static_cast<std::uint8_t>(content.value));
		S2C_BLOCK_PACKET block_p;
		block_p.header = { static_cast<std::uint16_t>(sizeof(block_p)), S2C_BLOCK };
		block_p.piece = static_cast<std::uint8_t>(content.value);
		std::memcpy(opener_buffer + opener_size, &block_p, sizeof(block_p));
		opener_size += sizeof(block_p);
	}
	else if (content.kind == BoxContentKind::ITEM) {
		opener.AddItem(static_cast<ItemKind>(content.value), content.grade);
		S2C_ITEM_PACKET item_p;
		item_p.header = { static_cast<std::uint16_t>(sizeof(item_p)), S2C_ITEM };
		item_p.kind = static_cast<std::uint8_t>(content.value);
		item_p.grade = content.grade;
		std::memcpy(opener_buffer + opener_size, &item_p, sizeof(item_p));
		opener_size += sizeof(item_p);
	}
	else {
		const std::uint8_t weapon = static_cast<std::uint8_t>(content.value);
		if (opener.GetAmmo(weapon)) {
			opener.AddSpareRounds(weapon, AMMO_BOX_ROUNDS[weapon]);
			S2C_AMMO_GAIN_PACKET gain_p;
			gain_p.header = { static_cast<std::uint16_t>(sizeof(gain_p)), S2C_AMMO_GAIN };
			gain_p.weapon = weapon;
			gain_p.rounds = static_cast<std::uint16_t>(AMMO_BOX_ROUNDS[weapon]);
			std::memcpy(opener_buffer + opener_size, &gain_p, sizeof(gain_p));
			opener_size += sizeof(gain_p);
		}
	}
	for (auto& room_player : room_players_) {
		auto session = FindSession(room_player);
		if (!session || room_player.GetRoomPlayerState() != RoomPlayerState::PLAY) continue;
		if (&room_player == &opener) session->SendPacket(opener_buffer, opener_size);
		else session->SendPacket(reinterpret_cast<char*>(&open_p), open_p.header.size);
	}
}

// F키로 열면 캡슐 중심에서 상자 중심까지 HINT_BOX_OPEN_DISTANCE 안이어야 한다.
// 쏴서 열면 선물이어야 하고, 총의 사거리 안이며, 캡슐 중심에서 상자까지 랜드스케이프나 총알을 막는 배치물(건물·나무·바위)이 가로막지 않아야 한다.
bool GameRoom::CheckBoxOpen(const Player& opener, std::uint16_t box, std::optional<std::uint8_t> shot_weapon) const
{
	if (!opener.HasMoveState()) return false;
	const BoxPlacement& placement = server_->GetBoxCandidateData().GetPlacements()[box_placement_indices_[box]];
	const float* location = opener.GetMoveState().location;
	const DirectX::XMFLOAT3 start(location[0], location[1], location[2]);
	if (!shot_weapon) return std::hypot(placement.center.x - start.x, placement.center.y - start.y, placement.center.z - start.z) <= HINT_BOX_OPEN_DISTANCE;

	const DirectX::XMFLOAT3 target(placement.location.x, placement.location.y, placement.location.z + HINT_BOX_SHOT_TARGET_HEIGHT);
	const float distance = std::hypot(target.x - start.x, target.y - start.y, target.z - start.z);
	const WeaponInfo* weapon = server_->GetHitboxData().GetWeaponInfo(*shot_weapon);
	if (placement.kind != BoxKind::GIFT || !weapon || !weapon->is_hitscan || distance > weapon->hitscan_distance) return false;
	if (distance <= HIT_OCCLUSION_MARGIN + HINT_BOX_SHOT_END_MARGIN) return true;
	const DirectX::XMVECTOR start_v = DirectX::XMLoadFloat3(&start);
	const DirectX::XMVECTOR direction_v = DirectX::XMVectorScale(DirectX::XMVectorSubtract(DirectX::XMLoadFloat3(&target), start_v), 1.0f / distance);
	DirectX::XMFLOAT3 segment_start;
	DirectX::XMFLOAT3 segment_end;
	DirectX::XMStoreFloat3(&segment_start, DirectX::XMVectorAdd(start_v, DirectX::XMVectorScale(direction_v, HIT_OCCLUSION_MARGIN)));
	DirectX::XMStoreFloat3(&segment_end, DirectX::XMVectorAdd(start_v, DirectX::XMVectorScale(direction_v, distance - HINT_BOX_SHOT_END_MARGIN)));
	const LandscapeData& landscape = server_->GetMapData().GetLandscape();
	const CollisionObjectData& collision_objects = server_->GetMapData().GetCollisionObjects();
	return !landscape.IsSegmentBlockedByLandscape(segment_start, segment_end) && !collision_objects.IsSegmentBlockedByObjects(segment_start, segment_end, TERRAIN_BLOCKS_VISIBILITY);
}

// 힌트 목록 번호의 힌트 패킷을 buffer에 쓰고 쓴 크기를 돌려준다.
int GameRoom::AppendHintPacket(char* buffer, std::uint32_t hint) const
{
	if (const DirectionHint* direction = std::get_if<DirectionHint>(&hints_[hint])) {
		S2C_DIRECTION_HINT_PACKET hint_p;
		hint_p.header = { static_cast<std::uint16_t>(sizeof(hint_p)), S2C_DIRECTION_HINT };
		hint_p.hint_id = static_cast<std::uint16_t>(hint);
		hint_p.area = direction->area;
		hint_p.direction = direction->direction;
		hint_p.is_true = direction->is_true;
		std::memcpy(buffer, &hint_p, sizeof(hint_p));
		return sizeof(hint_p);
	}
	const CountHint& count = std::get<CountHint>(hints_[hint]);
	S2C_COUNT_HINT_PACKET hint_p;
	hint_p.header = { static_cast<std::uint16_t>(sizeof(hint_p)), S2C_COUNT_HINT };
	hint_p.hint_id = static_cast<std::uint16_t>(hint);
	hint_p.object_kind = count.object_kind;
	hint_p.radius = count.radius;
	hint_p.count = count.count;
	std::memcpy(buffer, &hint_p, sizeof(hint_p));
	return sizeof(hint_p);
}

// 지우기 차례가 되면 보물 칸과 이미 지운 칸을 뺀 배제 판 칸 하나를 무작위로 골라 지운다. 지울 칸이 없으면 그 차례는 건너뛴다.
// 플레이 중인 전원에게 지운 칸과 바뀐 배제 판, 다음 지우기까지 남은 시간을 보낸다.
void GameRoom::ProcessAreaRemoval()
{
	if (match_phase_ != MatchPhase::STARTED || game_end_ms_ != 0 || tick_time_ms_ < next_area_removal_ms_) return;
	next_area_removal_ms_ += AREA_REMOVAL_INTERVAL_MS;
	const int treasure_cell = GetBlockCellIndex(treasure_placement_.location.x, treasure_placement_.location.y);
	std::vector<std::uint8_t> candidates;
	for (int cell = 0; cell < BLOCK_GRID_SIZE * BLOCK_GRID_SIZE; ++cell) {
		if (cell != treasure_cell && (removed_cells_ >> cell & 1) == 0) candidates.push_back(static_cast<std::uint8_t>(cell));
	}
	S2C_AREA_REMOVAL_PACKET removal_p;
	removal_p.header = { static_cast<std::uint16_t>(sizeof(removal_p)), S2C_AREA_REMOVAL };
	if (!candidates.empty()) {
		std::mt19937 random(CreateRandomSeed());
		removed_cells_ |= 1ULL << candidates[std::uniform_int_distribution<std::size_t>(0, candidates.size() - 1)(random)];
	}
	for (auto& room_player : room_players_) {
		auto session = FindSession(room_player);
		if (!session || room_player.GetRoomPlayerState() != RoomPlayerState::PLAY) continue;
		if (!candidates.empty()) {
			char buffer[sizeof(removal_p) + sizeof(S2C_EXCLUSION_BOARD_PACKET)];
			std::memcpy(buffer, &removal_p, sizeof(removal_p));
			session->SendPacket(buffer, sizeof(removal_p) + AppendExclusionBoard(buffer + sizeof(removal_p), room_player));
		}
		SendAreaRemovalTime(*session);
	}
}

int GameRoom::AppendExclusionBoard(char* buffer, Player& player) const
{
	const std::uint64_t board = player.GetKeptExcludedCells() | removed_cells_;
	if (board == player.GetSentExclusionBoard()) return 0;
	player.SetSentExclusionBoard(board);
	S2C_EXCLUSION_BOARD_PACKET board_p;
	board_p.header = { static_cast<std::uint16_t>(sizeof(board_p)), S2C_EXCLUSION_BOARD };
	board_p.excluded_cells = board;
	std::memcpy(buffer, &board_p, sizeof(board_p));
	return sizeof(board_p);
}

// 게임이 시작된 뒤 가진 블록을 하나마다 한 번 쓸 수 있고, 놓는 칸은 모두 판 안이며 아직 배제되지 않은 칸이어야 한다.
// 보물이 놓인 칸 안에 있으면 나머지 칸을, 없으면 놓인 칸을 배제하고, 쓴 사람에게 결과와 배제 판을 보낸다.
void GameRoom::ProcessBlockUse(Player& user, const C2S_BLOCK_USE_PACKET& request_p)
{
	if (match_phase_ != MatchPhase::STARTED || !user.CanUseBlock(request_p.piece) || request_p.rotation >= BLOCK_ROTATION_COUNT) return;
	const std::uint64_t cells = GetBlockCells(request_p.piece, request_p.rotation, request_p.column, request_p.row);
	if (cells == 0 || (cells & (user.GetKeptExcludedCells() | removed_cells_)) != 0) return;
	S2C_BLOCK_RESULT_PACKET result_p;
	result_p.header = { static_cast<std::uint16_t>(sizeof(result_p)), S2C_BLOCK_RESULT };
	result_p.piece = request_p.piece;
	result_p.is_inside = (cells >> GetBlockCellIndex(treasure_placement_.location.x, treasure_placement_.location.y) & 1) != 0;
	user.UseBlock(request_p.piece, result_p.is_inside ? ~cells : cells);
	char buffer[sizeof(result_p) + sizeof(S2C_EXCLUSION_BOARD_PACKET)];
	std::memcpy(buffer, &result_p, sizeof(result_p));
	const int size = sizeof(result_p) + AppendExclusionBoard(buffer + sizeof(result_p), user);
	if (auto session = FindSession(user)) session->SendPacket(buffer, size);
}

// 게임이 시작된 뒤 처음으로 보물 자리 가까이에서 곡괭이를 휘두르면 보물을 발견한다. 플레이 중인 방 전체에 알리고 게임이 끝날 시각을 정한다.
// 발견한 뒤에도 게임이 끝날 때까지 플레이는 그대로 하지만 힌트 제공은 멈춘다.
void GameRoom::DigTreasure(const Player& digger)
{
	if (match_phase_ != MatchPhase::STARTED || game_end_ms_ != 0 || !digger.HasMoveState()) return;
	const float* location = digger.GetMoveState().location;
	const float dx = location[0] - treasure_placement_.location.x;
	const float dy = location[1] - treasure_placement_.location.y;
	const float dz = location[2] - CHARACTER_CAPSULE_HALF_HEIGHT - treasure_placement_.location.z;
	if (dx * dx + dy * dy + dz * dz > TREASURE_DIG_DISTANCE * TREASURE_DIG_DISTANCE) return;
	game_end_ms_ = tick_time_ms_ + GAME_END_WAIT_MS;
	S2C_TREASURE_FOUND_PACKET found_p;
	found_p.header = { static_cast<std::uint16_t>(sizeof(found_p)), S2C_TREASURE_FOUND };
	found_p.player_id = digger.GetSessionKey().player_id;
	char buffer[sizeof(found_p) + sizeof(S2C_TREASURE_LOCATION_PACKET)];
	std::memcpy(buffer, &found_p, sizeof(found_p));
	AppendTreasureLocation(buffer + sizeof(found_p));
	for (auto& room_player : room_players_) {
		auto session = FindSession(room_player);
		if (session && room_player.GetRoomPlayerState() == RoomPlayerState::PLAY) session->SendPacket(buffer, sizeof(buffer));
	}
}

void GameRoom::ProcessTimeLimit()
{
	if (match_phase_ != MatchPhase::STARTED || game_end_ms_ != 0 || tick_time_ms_ < game_start_ms_ + GAME_TIME_LIMIT_MS) return;
	game_end_ms_ = tick_time_ms_ + GAME_END_WAIT_MS;
	S2C_TIME_OVER_PACKET over_p;
	over_p.header = { static_cast<std::uint16_t>(sizeof(over_p)), S2C_TIME_OVER };
	char buffer[sizeof(over_p) + sizeof(S2C_TREASURE_LOCATION_PACKET)];
	std::memcpy(buffer, &over_p, sizeof(over_p));
	AppendTreasureLocation(buffer + sizeof(over_p));
	for (auto& room_player : room_players_) {
		auto session = FindSession(room_player);
		if (session && room_player.GetRoomPlayerState() == RoomPlayerState::PLAY) session->SendPacket(buffer, sizeof(buffer));
	}
}

void GameRoom::AppendTreasureLocation(char* buffer) const
{
	S2C_TREASURE_LOCATION_PACKET location_p;
	location_p.header = { static_cast<std::uint16_t>(sizeof(location_p)), S2C_TREASURE_LOCATION };
	location_p.location[0] = treasure_placement_.location.x;
	location_p.location[1] = treasure_placement_.location.y;
	location_p.location[2] = treasure_placement_.location.z;
	location_p.yaw = treasure_placement_.yaw;
	std::memcpy(buffer, &location_p, sizeof(location_p));
}

// 기절이 끝난 플레이어마다 기절 해제와 그 사람의 보호 시간을 플레이 중인 방 전체에 알린다.
void GameRoom::ProcessStunRelease()
{
	for (auto& target : room_players_) {
		if (!target.TakeStunRelease(tick_time_ms_)) continue;
		S2C_STUN_RELEASE_PACKET release_p;
		release_p.header = { static_cast<std::uint16_t>(sizeof(release_p)), S2C_STUN_RELEASE };
		release_p.player_id = target.GetSessionKey().player_id;
		release_p.protection_ms = static_cast<std::uint32_t>(target.GetStunProtectionMs());
		for (auto& room_player : room_players_) {
			auto session = FindSession(room_player);
			if (session && room_player.GetRoomPlayerState() == RoomPlayerState::PLAY) session->SendPacket(reinterpret_cast<char*>(&release_p), release_p.header.size);
		}
	}
}

// 재장전이 끝난 무기마다 탄창 탄과 소지 탄을 재장전한 사람에게 보낸다.
void GameRoom::ProcessReloadDone()
{
	for (auto& room_player : room_players_) {
		auto session = FindSession(room_player);
		if (!session || room_player.GetRoomPlayerState() != RoomPlayerState::PLAY) continue;
		for (std::uint8_t weapon = 0; const WeaponAmmo* ammo = room_player.GetAmmo(weapon); ++weapon) {
			const WeaponInfo* info = server_->GetHitboxData().GetWeaponInfo(weapon);
			if (!info || !room_player.FinishReload(weapon, *info, tick_time_ms_)) continue;
			S2C_RELOAD_DONE_PACKET done_p;
			done_p.header = { static_cast<std::uint16_t>(sizeof(done_p)), S2C_RELOAD_DONE };
			done_p.weapon = weapon;
			done_p.rounds = static_cast<std::uint16_t>(ammo->rounds);
			done_p.spare_rounds = static_cast<std::uint16_t>(ammo->spare_rounds);
			session->SendPacket(reinterpret_cast<char*>(&done_p), done_p.header.size);
		}
	}
}

void GameRoom::StartAward()
{
	S2C_AWARD_PACKET award_p;
	award_p.header = { static_cast<std::uint16_t>(sizeof(award_p)), S2C_AWARD };
	int scene_count = 0;
	for (std::uint8_t category = 0; category < static_cast<std::uint8_t>(AwardCategory::COUNT); ++category) {
		const AwardCategory award = static_cast<AwardCategory>(category);
		std::vector<const Player*> ranked;
		for (const auto& room_player : room_players_) {
			if (FindSession(room_player) && room_player.GetRoomPlayerState() == RoomPlayerState::PLAY && room_player.GetAwardCount(award) > 0) ranked.push_back(&room_player);
		}
		std::sort(ranked.begin(), ranked.end(), [award](const Player* a, const Player* b) {
			if (a->GetAwardCount(award) != b->GetAwardCount(award)) return a->GetAwardCount(award) > b->GetAwardCount(award);
			return a->GetAwardReachMs(award) < b->GetAwardReachMs(award);
		});
		for (std::size_t rank = 0; rank < AWARD_RANK_COUNT; ++rank) {
			award_p.entries[category][rank] = rank < ranked.size() ? AWARD_ENTRY{ ranked[rank]->GetSessionKey().player_id, ranked[rank]->GetAwardCount(award) } : AWARD_ENTRY{ -1, 0 };
		}
		if (!ranked.empty()) ++scene_count;
	}
	if (scene_count == 0) {
		ClearGame();
		return;
	}
	for (auto& room_player : room_players_) {
		auto session = FindSession(room_player);
		if (session && room_player.GetRoomPlayerState() == RoomPlayerState::PLAY) session->SendPacket(reinterpret_cast<char*>(&award_p), award_p.header.size);
	}
	award_end_ms_ = tick_time_ms_ + scene_count * AWARD_SCENE_MS + AWARD_END_MARGIN_MS;
}

void GameRoom::SendAreaRemovalTime(Session& session) const
{
	S2C_AREA_REMOVAL_TIME_PACKET time_p;
	time_p.header = { static_cast<std::uint16_t>(sizeof(time_p)), S2C_AREA_REMOVAL_TIME };
	time_p.remaining_ms = static_cast<std::uint32_t>(std::max(0LL, next_area_removal_ms_ - tick_time_ms_));
	session.SendPacket(reinterpret_cast<char*>(&time_p), time_p.header.size);
}

// 이전에 인정한 위치와 새 위치를 비교한다. 발이 랜드스케이프보다 허용치 넘게 아래면 땅속이고,
// 두 위치의 캡슐 중심을 이은 선분이 캐릭터 이동을 막는 배치물(건물·나무·바위·경계 볼륨)을 지나면 관통이다.
// 새 위치의 캡슐이 이동을 막는 배치물에 허용치 넘게 파고들어도 인정하지 않는다. 게임 시작 전에는 시작 구역 밖으로 나간 위치도 인정하지 않는다. 첫 이동은 스폰 자리와 비교한다.
// 이번 이동이나 누적으로 갈 수 있는 거리보다 허용치 넘게 더 움직이면 속도 조작으로 보고 인정하지 않는다. 여기까지 어기면 되돌린다.
// 마지막으로 공중 높이와 땅 위라는 알림을 본다. 땅에서 떠나는 이동은 떠난 뒤의 공중 상태로, 공중에서 오는 이동은 지금 공중 상태로 높이를 본다.
// 점프는 알린 위치에서 시작하므로 그 위치가 땅 위여야 한다. is_slope_slide는 하강 중 직전과 이번 위치가 모두 비탈 접촉인지, is_open_air는 공중이고 어떤 면에도 닿지 않았는지다.
// 땅 위에 있다가 어기면 되돌리고, 공중에서 어기면 서버가 계산한 공중 위치로 맞춘다. 클라이언트 시각이 거꾸로 가거나 서버보다 너무 앞서도 되돌린다.
MoveCheckResult GameRoom::CheckMove(const Player& player, const MOVE_STATE& move_state, long long time_ms, bool is_slope_slide, bool is_open_air) const
{
	const float* next = move_state.location;
	if (!std::isfinite(next[0]) || !std::isfinite(next[1]) || !std::isfinite(next[2]) || move_state.air_phase > AirPhase::FALLING) return MoveCheckResult::ROLLBACK;
	const std::optional<long long> clock_drift = player.GetClockDrift(move_state, time_ms);
	if (!clock_drift || *clock_drift > CLOCK_MAX_AHEAD_MS) return MoveCheckResult::ROLLBACK;
	const LandscapeData& landscape = server_->GetMapData().GetLandscape();
	const CollisionObjectData& collision_objects = server_->GetMapData().GetCollisionObjects();
	const std::optional<float> ground = landscape.GetGroundHeight(next[0], next[1]);
	if (ground && next[2] - CHARACTER_CAPSULE_HALF_HEIGHT < *ground - MOVE_TOLERANCE) return MoveCheckResult::ROLLBACK;
	// 캡슐을 허용치만큼 줄여도 겹치면 허용치보다 깊이 파고든 것이다.
	if (collision_objects.IsCapsuleOverlappingObjects({ next[0], next[1], next[2] }, CHARACTER_CAPSULE_RADIUS - MOVE_TOLERANCE, CHARACTER_CAPSULE_HALF_HEIGHT - MOVE_TOLERANCE, TERRAIN_BLOCKS_PAWN)) return MoveCheckResult::ROLLBACK;
	if (match_phase_ != MatchPhase::STARTED && (next[0] < START_AREA_MIN[0] || next[0] > START_AREA_MAX[0] || next[1] < START_AREA_MIN[1] || next[1] > START_AREA_MAX[1])) return MoveCheckResult::ROLLBACK;
	const float* previous = player.GetMoveState().location;
	if (player.GetMoveStepExcess(move_state, time_ms) > MOVE_MAX_STEP_EXCESS_DISTANCE || player.GetMoveExcess(move_state, time_ms) > MOVE_MAX_EXCESS_DISTANCE) return MoveCheckResult::ROLLBACK;
	if (collision_objects.IsSegmentBlockedByObjects({ previous[0], previous[1], previous[2] }, { next[0], next[1], next[2] }, TERRAIN_BLOCKS_PAWN)) return MoveCheckResult::ROLLBACK;
	const AirState air = player.GetAirState(time_ms);
	const AirState judged = air.phase == AirPhase::GROUND ? player.GetNextAirState(move_state, time_ms, false) : air;
	const MoveCheckResult air_failure = air.phase == AirPhase::GROUND ? MoveCheckResult::ROLLBACK : MoveCheckResult::AIR_CORRECTION;
	// 점프는 입력이라 기절 중에는 할 수 없다.
	if (air.phase == AirPhase::GROUND && move_state.air_phase == AirPhase::RISING && (!IsOnGround(next) || player.IsInputLocked(time_ms))) return air_failure;
	// 비탈을 타고 떨어지는 중이면 비탈이 낙하를 늦추므로 낙하 곡선 대신 직전 위치보다 올라가지 않는지만 본다.
	// 언리얼 캐릭터는 떨어지는 중 면에 부딪혀 위로 꺾일 이동을 수평으로 바꾸므로, 비탈을 타고 올라가지 않는다.
	if (judged.phase == AirPhase::FALLING && is_slope_slide) {
		if (next[2] > previous[2]) return air_failure;
	}
	else if (judged.phase != AirPhase::GROUND && !IsAirHeightAllowed(judged, next[2], time_ms)) return air_failure;
	// 허공에서는 직전 두 상태로 예측한 자유 낙하 높이에서 벗어나지 않아야 한다. 닿은 면이 없으니 늦게 떨어질 이유가 없다.
	if (is_open_air) {
		const std::optional<std::pair<float, float>> predicted = player.GetPredictedAirZ(move_state);
		if (predicted && std::abs(next[2] - predicted->first) > predicted->second) return air_failure;
	}
	if (move_state.air_phase == AirPhase::GROUND && !IsOnGround(next)) return air_failure;
	return MoveCheckResult::ACCEPTED;
}

// 캡슐을 GROUND_CHECK_DEPTH만큼 내리고 반지름을 GROUND_CHECK_RADIUS_MARGIN만큼 줄여 랜드스케이프나 캐릭터를 막는 배치물에 닿으면 땅 위로 본다.
bool GameRoom::IsOnGround(const float location[3]) const
{
	const std::optional<float> ground = server_->GetMapData().GetLandscape().GetGroundHeight(location[0], location[1]);
	if (ground && location[2] - CHARACTER_CAPSULE_HALF_HEIGHT <= *ground + GROUND_CHECK_DEPTH) return true;
	return server_->GetMapData().GetCollisionObjects().IsCapsuleOverlappingObjects({ location[0], location[1], location[2] - GROUND_CHECK_DEPTH },
		CHARACTER_CAPSULE_RADIUS - GROUND_CHECK_RADIUS_MARGIN, CHARACTER_CAPSULE_HALF_HEIGHT, TERRAIN_BLOCKS_PAWN);
}

// 캡슐 아래 반구 중심에서 그 자리 랜드스케이프 면까지의 거리(cm)와 면 법선의 위쪽 성분(cos 기울기). 면은 앞뒤·좌우 높이로 구한 기울기의 평면으로 본다.
static std::optional<std::pair<float, float>> GetLandscapeContact(const LandscapeData& landscape, const float location[3])
{
	const std::optional<float> ground = landscape.GetGroundHeight(location[0], location[1]);
	const std::optional<float> ground_x0 = landscape.GetGroundHeight(location[0] - SLOPE_SAMPLE_DISTANCE, location[1]);
	const std::optional<float> ground_x1 = landscape.GetGroundHeight(location[0] + SLOPE_SAMPLE_DISTANCE, location[1]);
	const std::optional<float> ground_y0 = landscape.GetGroundHeight(location[0], location[1] - SLOPE_SAMPLE_DISTANCE);
	const std::optional<float> ground_y1 = landscape.GetGroundHeight(location[0], location[1] + SLOPE_SAMPLE_DISTANCE);
	if (!ground || !ground_x0 || !ground_x1 || !ground_y0 || !ground_y1) return std::nullopt;
	const float slope_x = (*ground_x1 - *ground_x0) / (2.0f * SLOPE_SAMPLE_DISTANCE);
	const float slope_y = (*ground_y1 - *ground_y0) / (2.0f * SLOPE_SAMPLE_DISTANCE);
	const float normal_z = 1.0f / std::sqrt(1.0f + slope_x * slope_x + slope_y * slope_y);
	const float sphere_z = location[2] - CHARACTER_CAPSULE_HALF_HEIGHT + CHARACTER_CAPSULE_RADIUS;
	return std::pair<float, float>{ (sphere_z - *ground) * normal_z, normal_z };
}

// 비탈 접촉: 공중에서 캡슐 아래 반구가 걸을 수 없는 가파른 비탈에 닿아 있는지 본다. 걸을 수 있는 면에서는 클라이언트가 바로 걷기로 바뀌므로 비탈 접촉으로 보지 않는다.
// 그 자리 랜드스케이프 기울기가 STEEP_SLOPE_MIN_DEGREES 이상이고, 그 기울기로 본 면까지의 거리가 캡슐 아래 반구 중심에서 반지름 + MOVE_TOLERANCE 이하면 비탈 접촉이다.
// 배치물은 면 기울기를 알 수 없어 비탈 접촉으로 보지 않는다.
bool GameRoom::IsTouchingSteepSlope(const float location[3]) const
{
	const std::optional<std::pair<float, float>> contact = GetLandscapeContact(server_->GetMapData().GetLandscape(), location);
	return contact && contact->second <= std::cos(DirectX::XMConvertToRadians(STEEP_SLOPE_MIN_DEGREES)) && contact->first <= CHARACTER_CAPSULE_RADIUS + MOVE_TOLERANCE;
}

// 캡슐을 AIR_CONTACT_MARGIN만큼 키웠을 때 랜드스케이프(기울기와 관계없이), 캐릭터를 막는 배치물, 다른 플레이어 캡슐에 닿는지 본다.
bool GameRoom::IsTouchingSurface(const Player& player, const float location[3]) const
{
	const std::optional<std::pair<float, float>> contact = GetLandscapeContact(server_->GetMapData().GetLandscape(), location);
	if (contact && contact->first <= CHARACTER_CAPSULE_RADIUS + AIR_CONTACT_MARGIN) return true;
	if (server_->GetMapData().GetCollisionObjects().IsCapsuleOverlappingObjects({ location[0], location[1], location[2] },
		CHARACTER_CAPSULE_RADIUS + AIR_CONTACT_MARGIN, CHARACTER_CAPSULE_HALF_HEIGHT + AIR_CONTACT_MARGIN, TERRAIN_BLOCKS_PAWN)) return true;
	// 두 캡슐은 서 있으므로 높이 차이에서 원기둥 부분 길이를 뺀 만큼이 위아래 거리다.
	for (const Player& other : room_players_) {
		if (&other == &player || other.GetRoomPlayerState() != RoomPlayerState::PLAY || !other.HasMoveState()) continue;
		const float* other_location = other.GetMoveState().location;
		const float vertical = std::max(0.0f, std::abs(location[2] - other_location[2]) - 2.0f * (CHARACTER_CAPSULE_HALF_HEIGHT - CHARACTER_CAPSULE_RADIUS));
		if (std::hypot(location[0] - other_location[0], location[1] - other_location[1], vertical) <= 2.0f * CHARACTER_CAPSULE_RADIUS + AIR_CONTACT_MARGIN) return true;
	}
	return false;
}

// 이동을 인정하지 않고 서버 위치로 맞춘다. 수평 속도는 없애고, 공중이면 수직 속도는 서버 곡선을 그대로 따른다.
// is_on_air_curve이고 공중이면 마지막으로 인정한 수평 위치에서 곡선의 지금 높이로 맞춘다. 그 높이가 땅 아래면 땅에 내려놓는다.
// 그 위치까지 바닥 배치물 등에 막히면 마지막으로 인정한 위치에서 속도 0으로 떨어지기 시작한 것으로 본다. 아래가 막혀 있어 곧 그 위에 내려선다.
// 아니면 마지막으로 인정한 위치로 되돌린다. 맞춘 위치는 다른 플레이어에게 다음 틱에 중계하고, 본인에게는 그 위치로 옮기라고 보낸다.
void GameRoom::CorrectMove(Player& player, Session& session, bool is_on_air_curve)
{
	AirState air = player.GetAirState(tick_time_ms_);
	const float* previous = player.GetMoveState().location;
	float location[3] = { previous[0], previous[1], previous[2] };
	if (is_on_air_curve && air.phase != AirPhase::GROUND) {
		float curve_z = GetAirHeight(air, tick_time_ms_);
		AirState curve_air = air;
		const std::optional<float> ground = server_->GetMapData().GetLandscape().GetGroundHeight(location[0], location[1]);
		if (ground && curve_z - CHARACTER_CAPSULE_HALF_HEIGHT < *ground) {
			curve_z = *ground + CHARACTER_CAPSULE_HALF_HEIGHT;
			curve_air = {};
		}
		if (server_->GetMapData().GetCollisionObjects().IsSegmentBlockedByObjects({ previous[0], previous[1], previous[2] }, { previous[0], previous[1], curve_z }, TERRAIN_BLOCKS_PAWN)) {
			air = { AirPhase::FALLING, previous[2], tick_time_ms_ };
		}
		else {
			location[2] = curve_z;
			air = curve_air;
		}
	}
	const float velocity_z = air.phase == AirPhase::GROUND ? 0.0f : GetAirVelocityZ(air, tick_time_ms_);
	S2C_MOVE_CORRECTION_PACKET correction_p;
	correction_p.header = { static_cast<std::uint16_t>(sizeof(correction_p)), S2C_MOVE_CORRECTION };
	correction_p.correction_id = player.StartMoveCorrection(location, velocity_z, air, tick_time_ms_);
	std::memcpy(correction_p.location, location, sizeof(correction_p.location));
	correction_p.velocity_z = velocity_z;
	correction_p.air_phase = air.phase;
	session.SendPacket(reinterpret_cast<char*>(&correction_p), correction_p.header.size);
}

// 전원이 로딩을 마치거나 로딩 제한 시간이 지나면 시작 대기를 알리고, 대기 시간이 지나면 게임 시작을 알린다.
void GameRoom::ProcessGameStart()
{
	if (match_phase_ == MatchPhase::STARTED) return;
	const auto send_to_playing = [&](char* packet, int packet_size) {
		for (auto& room_player : room_players_) {
			auto session = FindSession(room_player);
			if (session && room_player.GetRoomPlayerState() == RoomPlayerState::PLAY) session->SendPacket(packet, packet_size);
		}
	};
	if (match_phase_ == MatchPhase::LOADING) {
		bool has_loading_player = false;
		for (auto& room_player : room_players_) {
			if (FindSession(room_player) && room_player.GetRoomPlayerState() == RoomPlayerState::LOADING) has_loading_player = true;
		}
		if (has_loading_player && tick_time_ms_ < loading_deadline_ms_) return;
		match_phase_ = MatchPhase::READY;
		game_start_ms_ = tick_time_ms_ + GAME_READY_WAIT_MS;
		S2C_GAME_READY_PACKET ready_p;
		ready_p.header = { static_cast<std::uint16_t>(sizeof(ready_p)), S2C_GAME_READY };
		ready_p.remaining_ms = GAME_READY_WAIT_MS;
		send_to_playing(reinterpret_cast<char*>(&ready_p), ready_p.header.size);
		return;
	}
	if (tick_time_ms_ < game_start_ms_) return;
	match_phase_ = MatchPhase::STARTED;
	next_area_removal_ms_ = tick_time_ms_ + AREA_REMOVAL_INTERVAL_MS;
	S2C_GAME_START_PACKET game_start_p;
	game_start_p.header = { static_cast<std::uint16_t>(sizeof(game_start_p)), S2C_GAME_START };
	game_start_p.remaining_ms = GAME_TIME_LIMIT_MS;
	for (auto& room_player : room_players_) {
		auto session = FindSession(room_player);
		if (!session || room_player.GetRoomPlayerState() != RoomPlayerState::PLAY) continue;
		session->SendPacket(reinterpret_cast<char*>(&game_start_p), game_start_p.header.size);
		SendAreaRemovalTime(*session);
	}
}

void GameRoom::FindNewHost()
{
	bool has_found_host = false;
	auto& room_players = room_players_;

	if (GetCurrentPlayerCount() != 0) {
		for (auto& room_player : room_players) {
			auto session = FindSession(room_player);
			if (!session) continue;
			host_id_ = session->GetSessionKey().player_id;
			if (room_state_ == RoomState::WAIT) room_player.SetRoomPlayerState(RoomPlayerState::WAIT); // 게임 중이 아닐 때, 호스트가 나갔을 때 레디 상태인 사람이 호스트가 되면, 레디 상태를 풀어줘야함
			has_found_host = true;
			break;
		}
	}

	if (has_found_host) {
		S2C_UPDATE_HOST_PACKET host_p;
		host_p.header.size = static_cast<std::uint16_t>(sizeof(host_p));
		host_p.header.type = S2C_UPDATE_HOST;
		host_p.new_host_id = host_id_;
		Broadcast(reinterpret_cast<char*>(&host_p));
	}

	else if (GetCurrentPlayerCount() == 0) {
		BeginRoomDelete();
	}
	else host_id_ = -1;
}

int GameRoom::FindHostIndex(int host_id)
{
	auto& room_players = room_players_;
	for (int i = 0; i < room_players.size(); ++i) {
		auto session = FindSession(room_players[i]);
		if (!session) continue;
		if (session->GetSessionKey().player_id == host_id) return i;
	}
	return -1;
}
