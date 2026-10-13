#pragma once
#include <chrono>
#include <atomic>
#include <vector>
#include <array>
#include <string>
#include <cstdint>
#include <utility>
#include <mutex>
#include "ConcurrentTaskQueue.h"
#include "Player.h"
#include "box_types.h"
#include "hint_types.h"
#include "tower_types.h"
#include "treasure_types.h"
class Session;

class GameServer;
class GameThread;
class GameThreadManager;


struct RoomTask
{
	RoomTaskType task_type = RoomTaskType::NONE;
	SessionKey session_key; // 세션이 먼저 정리돼도 지연된 방 작업이 같은 세대의 플레이어만 처리하도록 보관한다.
	RoomKey room_key = 0; // 방 자리가 재사용돼도 다른 방에 작업이 적용되지 않도록 대상 방의 키를 보관한다.
	char room_password[MAX_ROOM_PASSWORD_SIZE] = {};
	int target_player_id = -1;
	bool is_matching = false; // 빠른 매칭에서 온 입장 요청이면 실패 시 로비가 아니라 매칭 상태로 되돌린다.
};

// 방 이름과 비밀번호는 패킷과 같은 고정 길이 배열로 두고, 남는 부분은 0으로 채운다. 비밀번호가 모두 0이면 공개방이다.
struct RoomInitData {
	RoomKey room_key = 0;
	char room_name[MAX_ROOM_NAME_SIZE] = {};
	char room_password[MAX_ROOM_PASSWORD_SIZE] = {};
};

struct RoomInfoSnapshot {
	RoomState room_state = RoomState::EMPTY;
	RoomKey room_key = 0;
	int max_player_count = 0;
	int current_player_count = 0;
	char room_name[MAX_ROOM_NAME_SIZE] = {};
	bool is_private = false;
};

// 방은 서버의 고정 배열에 미리 만들어 두고 자리를 재사용한다.
// 자리 상태: EMPTY → INITIALIZING(생성 중) → WAIT/PLAY → WAITING_DELETE → DELETE_POST → 초기화 → EMPTY
// 외부에서 들어오는 방 변경 작업은 큐에 보관하고, 방 처리권을 얻은 스레드만 room_players를 변경한다.
class GameRoom
{
	friend class GameThread;
	friend class GameThreadManager;
	friend class GameServer;

private:
	GameServer* server_ = nullptr;
	std::atomic<RoomState> room_state_{ RoomState::EMPTY };
	// 빈 자리와 초기화 중에는 FREE로 두어 게임 스레드의 처리 점유(COMPLETE → PROCESSING)를 막는다.
	std::atomic<RoomProcessState> processing_state_{ RoomProcessState::FREE };
	// 방 삭제와 남은 작업 정리를 동기화한다.
	std::mutex room_mutex_;
	ConcurrentTaskQueue<RoomTask> room_tasks_;

	int room_index_ = -1;
	// 방 키는 방이 만들어질 때마다 새 난수로 바뀌며, 방 자리 재사용을 구분하는 식별자다.
	// 초기화할 때 0으로 먼저 바꾸고, 새 방 정보를 모두 기록한 뒤 마지막에 새 키를 공개한다.
	// 방 정보를 읽는 쪽은 읽기 전후의 키를 비교해 그 사이 재사용이 일어났는지 알아챈다.
	std::atomic<RoomKey> room_key_{ 0 };
	char room_name_[MAX_ROOM_NAME_SIZE] = {};
	char room_password_[MAX_ROOM_PASSWORD_SIZE] = {};
	// 다른 스레드(매칭)가 방 생성·초기화와 겹쳐 읽을 수 있으므로 비공개 여부는 원자 변수로 따로 둔다.
	std::atomic<bool> is_private_{ false };
	std::array<Player, MAX_ROOM_PLAYER_COUNT> room_players_;
	int host_id_ = -1;
	std::atomic<char> current_player_count_{ 0 };
	// 현재 처리 중인 틱 시각. 받은 이동 상태와 동작에 서버 시각으로 기록한다.
	long long tick_time_ms_ = 0;
	// 이번 틱에 받은 동작. 덮어쓰지 않고 틱 끝에 이동 상태와 함께 보낸다.
	std::array<S2C_ACTION_PACKET, MAX_ROOM_ACTIONS_PER_TICK> pending_actions_{};
	std::size_t pending_action_count_ = 0;
	// 전원 로딩 완료를 기다리는 마감 시각, 실제 게임 시작 시각(0이면 아직 시작 대기 전), 게임 시작 여부
	long long loading_deadline_ms_ = 0;
	long long game_start_ms_ = 0;
	bool is_game_started_ = false;
	// 플레이어 자세의 애니메이션 진행을 마지막으로 계산한 틱 시각
	long long last_pose_tick_ms_ = 0;
	// 이번 게임에 놓인 탑의 지형 후보 배치물 번호. 이동·명중 검사가 놓인 탑만 보게 넘긴다.
	std::vector<std::uint32_t> placed_objects_;
	// 게임 시작 때 고른 탑 후보 번호·종류와, 그 탑 배치를 담은 패킷. 패킷은 로딩을 마친 플레이어에게 스폰보다 먼저 보낸다.
	std::vector<TowerPlacement> tower_placements_;
	S2C_TOWER_LAYOUT_PACKET tower_layout_p_{};
	// 게임 시작 때 고른 상자 자리 번호(유적 상자 다음 자연 상자 순서)와 그 배치 패킷. 상자 번호는 이 목록의 순서다.
	// 패킷은 모두 닫힌 상태로 만들어 두고, 게임 시작 전에 로딩을 마친 플레이어에게 탑 배치 패킷과 함께 보낸다. 시작 뒤에 들어온 플레이어에게는 그때 열림 상태로 새로 만들어 보낸다.
	std::vector<std::uint32_t> box_placement_indices_;
	std::vector<char> box_layout_packet_;
	// 상자마다 열렸는지. 상자 번호 순서다.
	std::vector<bool> is_box_opened_;
	// 게임 시작 때 정한 최종 보물 배치
	TreasurePlacement treasure_placement_{};
	// 게임 시작 때 최종 보물 위치로 만든 이번 게임의 힌트 목록
	std::vector<HINT_ENTRY> hints_;
	// 상자마다 넣은 내용물(힌트 또는 탄약). 상자 번호 순서다.
	std::vector<BoxContent> box_contents_;
	// 이번 게임에서 떨어진 힌트. 드랍 번호 순서이고, 주워도 지우지 않고 주웠다고만 표시한다.
	std::vector<HintDrop> hint_drops_;
	// 이번 게임에서 모두에게 공개한 힌트의 힌트 목록 번호, 지난 공개 차례 수, 다음 공개 시각
	std::vector<std::uint32_t> public_hints_;
	int public_hint_turn_ = 0;
	long long next_public_hint_ms_ = 0;
	// 보물을 발견해 게임이 끝나는 시각. 0이면 아직 발견하지 않았다.
	long long game_end_ms_ = 0;

	Session* FindSession(const Player& player) const;
	Player* FindPlayer(SessionKey session_key);
	void LeaveToLobby(SessionKey session_key, RoomExitType exit_type);
	void CompletePlayerRemoval(SessionKey session_key, RoomExitType exit_type);
	bool IsOwnedPlayer(const Session& session, SessionKey session_key);
	void BeginRoomDelete();
	void TryPostRoomDelete();
	void ProcessSessionTasks();
	void ProcessRoomTasks();
	void ProcessJoinTask(const RoomTask& task);
	void ProcessRoomTask(const RoomTask& task);
	int AddPlayer(Session& new_session, SessionKey session_key, const char* room_password);
	void ProcessSpecificRoomTask(const RoomTask& task);
	void ProcessGameTick(long long tick_time_ms);
	void ProcessLoadedPlayers();
	void ProcessGameStart();
	bool AcceptAction(Player& player, Session& session, const C2S_ACTION_PACKET& request_p);
	void ProcessHit(Player& shooter, const C2S_HIT_PACKET& request_p);
	void AddStunGauge(Player& target, const C2S_HIT_PACKET& request_p);
	bool CheckHit(const Player& shooter, const Player& target, const C2S_HIT_PACKET& request_p) const;
	void ProcessBoxOpen(Player& opener, const C2S_BOX_OPEN_PACKET& request_p);
	bool CheckBoxOpen(const Player& opener, const C2S_BOX_OPEN_PACKET& request_p) const;
	int AppendHintPackets(char* buffer, std::uint32_t hint) const;
	void DropHint(Player& target);
	void ProcessPublicHint();
	void DigTreasure(const Player& digger);
	// 게임 제한시간이 끝났는데 아무도 보물을 찾지 못했으면 보물 위치를 알리고 게임이 끝날 시각을 정한다.
	void ProcessTimeLimit();
	void ProcessSearchStart(Player& searcher, const C2S_SEARCH_PACKET& request_p);
	void ProcessSearches();
	// 플레이어의 지금 배제 판. 가진 방향 힌트는 배제 영역이 칸 전체를 덮는 칸만 넣고, 블록 결과와 돋보기 탐색 결과로 배제된 칸을 더한다.
	std::uint64_t ComputeExclusionBoard(const Player& player) const;
	// 배제 판이 마지막으로 보낸 것과 다르면 배제 판 패킷을 buffer에 쓰고 쓴 크기를 돌려준다. 같으면 쓰지 않고 0을 돌려준다.
	int AppendExclusionBoard(char* buffer, Player& player) const;
	void ProcessBlockUse(Player& user, const C2S_BLOCK_USE_PACKET& request_p);
	void SendPublicHintTime(Session& session) const;
	void ProcessHintPickup(Player& picker, const C2S_HINT_PICKUP_PACKET& request_p);
	bool CheckHintPickup(const Player& picker, const HintDrop& drop) const;
	bool CheckMove(const Player& player, const MOVE_STATE& move_state) const;
	void RollbackMove(Player& player, Session& session);
	void StartGame(int requester_id);
	void PlaceTreasure();
	void GenerateHints();
	void PlaceBoxes(int player_count);
	// 상자 배치 패킷들을 이어 붙여 만든다. 패킷 하나가 패킷 크기 칸에 담을 수 있는 크기를 넘지 않게 상자를 나눠 담는다.
	std::vector<char> BuildBoxLayoutPacket() const;
	void FindNewHost(); // 방장이 나갔을 때 새로운 방장 찾기
	int FindHostIndex(int host_id); // 현재 호스트의 인덱스를 반환
	void TogglePlayerReady(int player_id);
	void KickPlayer(int requester_id, int kick_player_id);

public:
	GameRoom() = default;
	~GameRoom();

	// 방 자리 관리
	void AttachToServer(GameServer* server, int room_index);
	bool TryClaim();
	void Initialize(const RoomInitData& data);
	bool TryBeginReset();
	void Reset();

	RoomKey GetRoomKey() const { return room_key_.load(std::memory_order_acquire); }
	RoomState GetRoomState() const { return room_state_.load(); }
	int GetRoomIndex() const { return room_index_; }
	bool IsPrivate() const { return is_private_.load(); }
	int GetCurrentPlayerCount() const;
	bool TryGetRoomInfoSnapshot(RoomInfoSnapshot& snapshot) const;

	// 공통
	void HandlePacket(char* packet, Session& request_session);
	void ProcessRoomTick(long long tick_time_ms);
	void SendCreateRoom(Session& session);
	bool AddHostSession(SessionKey session_key);
	bool AddRoomTask(RoomTask task);
	void CompleteRoomInitialization();
	void StoreRoomState(const RoomState new_state);
	bool TryChangeRoomState(RoomState expected, RoomState desired);
	
	void ClearGame();

	void BroadcastPackets();
	void Broadcast(char* packet);
};
