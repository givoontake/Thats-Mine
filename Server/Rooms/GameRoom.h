#pragma once
#include <chrono>
#include <atomic>
#include <vector>
#include <array>
#include <string>
#include <cstdint>
#include <optional>
#include <utility>
#include <mutex>
#include "ConcurrentTaskQueue.h"
#include "Player.h"
#include "box_types.h"
#include "hint_types.h"
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
	// 이번 틱에 받은 동작·재장전·입력 변화의 중계 패킷을 받은 순서대로 이어 둔 것. 덮어쓰지 않고 틱 끝에 이동 상태와 함께 보낸다.
	std::vector<char> pending_relays_;
	std::size_t pending_relay_count_ = 0;
	// 시작 단계, 전원 로딩 완료를 기다리는 마감 시각, 실제 게임 시작 시각(시작 대기에 들어갈 때 정한다)
	MatchPhase match_phase_ = MatchPhase::LOADING;
	long long loading_deadline_ms_ = 0;
	long long game_start_ms_ = 0;
	// 플레이어 자세의 애니메이션 진행을 마지막으로 계산한 틱 시각
	long long last_pose_tick_ms_ = 0;
	// 게임 시작 때 고른 상자 자리 번호(유적 상자 다음 자연 상자 순서)와 그 배치 패킷. 상자 번호는 이 목록의 순서다.
	// 패킷은 모두 닫힌 상태로 만들어 두고, 게임 시작 전에 로딩을 마친 플레이어에게 보낸다. 시작 뒤에 들어온 플레이어에게는 그때 열림 상태로 새로 만들어 보낸다.
	std::vector<std::uint32_t> box_placement_indices_;
	std::vector<char> box_layout_packet_;
	// 상자마다 열렸는지. 상자 번호 순서다.
	std::vector<bool> is_box_opened_;
	// 게임 시작 때 정한 최종 보물 배치
	TreasurePlacement treasure_placement_{};
	// 게임 시작 때 최종 보물 위치로 만든 이번 게임의 힌트 목록
	std::vector<Hint> hints_;
	// 상자마다 넣은 내용물(힌트 또는 탄약). 상자 번호 순서다.
	std::vector<BoxContent> box_contents_;
	// 이번 게임의 탄 퍼짐 시드(shot_spread.h). 게임마다 새로 정한다.
	std::uint64_t shot_seed_ = 0;
	// 이번 게임에서 영역 지우기로 지운 배제 판 칸 모음과 다음 지우기 시각. 지운 칸은 플레이 중인 전원의 배제 판에 들어간다.
	std::uint64_t removed_cells_ = 0;
	long long next_area_removal_ms_ = 0;
	// 보물을 발견해 게임이 끝나는 시각. 0이면 아직 발견하지 않았다.
	long long game_end_ms_ = 0;
	// 시상식이 끝나 방으로 돌아가는 시각. 0이면 시상식 중이 아니다.
	long long award_end_ms_ = 0;

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
	// 동작이나 재장전을 weapon으로 인정하는지 본다. 동작은 손에 든 장비, 재장전은 재장전 패킷의 무기다.
	bool AcceptAction(Player& player, Session& session, PlayerAction action, std::uint8_t weapon);
	// 중계 패킷을 이번 틱 중계에 넣는다. 이번 틱 중계 수가 다 찼으면 넣지 않고 false를 돌려준다.
	bool QueueRelay(const void* packet, int packet_size);
	// 그 플레이어의 이동 키 입력·조준·장비 패킷을 buffer에 쓰고 쓴 크기를 돌려준다. 스폰 뒤에 이어 붙인다.
	int AppendInputState(char* buffer, const Player& player) const;
	void ProcessHit(Player& shooter, const C2S_HIT_PACKET& request_p);
	void ProcessShotTrace(Player& shooter, const C2S_SHOT_TRACE_PACKET& request_p);
	void AddStunGauge(Player& shooter, Player& target, const C2S_HIT_PACKET& request_p);
	bool CheckHit(const Player& shooter, const Player& target, const C2S_HIT_PACKET& request_p) const;
	bool CheckShotCamera(const Player& shooter, const DirectX::XMFLOAT3& camera, const DirectX::XMFLOAT3& hit_point) const;
	bool IsShotPathBlocked(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& end) const;
	bool CheckShotSpread(const Player& shooter, const C2S_HIT_PACKET& request_p, const DirectX::XMFLOAT3& direction) const;
	void ProcessBoxOpen(Player& opener, std::uint16_t box, std::optional<std::uint8_t> shot_weapon);
	bool CheckBoxOpen(const Player& opener, std::uint16_t box, std::optional<std::uint8_t> shot_weapon) const;
	// 힌트 번호 hint의 종류별 힌트 패킷을 buffer에 쓰고 쓴 크기를 돌려준다.
	int AppendHintPacket(char* buffer, std::uint32_t hint) const;
	void ProcessAreaRemoval();
	void ProcessStunRelease();
	void DigTreasure(const Player& digger);
	// 게임 제한시간이 끝났는데 아무도 보물을 찾지 못했으면 보물 위치를 알리고 게임이 끝날 시각을 정한다.
	void ProcessTimeLimit();
	// 보물 위치 패킷을 buffer에 쓴다. 보물 발견이나 시간 종료 뒤에 이어 붙인다.
	void AppendTreasureLocation(char* buffer) const;
	// 게임이 끝나고 GAME_END_WAIT_MS가 지나면 기록이 있는 부문마다 순위를 보내고 시상식이 끝날 시각을 정한다. 기록이 있는 부문이 없으면 바로 방으로 돌려보낸다.
	void StartAward();
	// 배제 판(블록 결과·돋보기 탐색 결과로 배제된 칸과 영역 지우기로 지운 칸)이 마지막으로 보낸 것과 다르면 배제 판 패킷을 buffer에 쓰고 쓴 크기를 돌려준다. 같으면 쓰지 않고 0을 돌려준다.
	int AppendExclusionBoard(char* buffer, Player& player) const;
	void ProcessBlockUse(Player& user, const C2S_BLOCK_USE_PACKET& request_p);
	void SendAreaRemovalTime(Session& session) const;
	MoveCheckResult CheckMove(const Player& player, const MOVE_STATE& move_state, long long time_ms, bool is_slope_slide, bool is_open_air) const;
	bool IsOnGround(const float location[3]) const;
	bool IsTouchingSteepSlope(const float location[3]) const;
	bool IsTouchingSurface(const Player& player, const float location[3]) const;
	void CorrectMove(Player& player, Session& session, bool is_on_air_curve);
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
	void HandlePacket(char* packet, Session& request_session, long long receive_time_ms);
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
