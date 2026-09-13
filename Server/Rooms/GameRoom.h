#pragma once
#include <chrono>
#include <atomic>
#include <vector>
#include <array>
#include <string>
#include <cstdint>
#include <utility>
#include <optional>
#include <mutex>
#include "ConcurrentTaskQueue.h"
#include "Player.h"
#include "Session.h"

class GameServer;
class GameThread;
class GameThreadManager;


struct RoomTask
{
	RoomTaskType task_type = RoomTaskType::NONE;
	SessionKey session_key; // 세션이 먼저 정리돼도 지연된 방 작업이 같은 세대의 플레이어만 처리하도록 보관한다.
	std::string room_password;
	int target_player_id = -1;
	int matching_max_player_count = -1;
};

struct PublicRoomInitData {
	RoomKey room_key = 0;
	int room_index = -1;
	std::string room_name;
};

struct PrivateRoomInitData {
	RoomKey room_key = 0;
	int room_index = -1;
	std::string room_name;
	std::string room_password;
};

struct RoomInfoSnapshot {
	RoomState room_state = RoomState::EMPTY;
	RoomKey room_key = 0;
	int max_player_count = 0;
	int current_player_count = 0;
	std::string room_name;
	bool is_private = false;
};

// 외부에서 들어오는 방 변경 작업은 큐에 보관하고, 방 처리권을 얻은 스레드만 room_players를 변경한다.
class GameRoom
{
	friend class GameThread;
	friend class GameThreadManager;
	friend class GameServer;

private:
	GameServer* server_;
	std::atomic<RoomState> room_state_;
	std::atomic<RoomProcessState> processing_state_{ RoomProcessState::PROCESSING };
	// 방 삭제와 남은 작업 정리를 동기화한다.
	std::mutex room_mutex_;
	ConcurrentTaskQueue<RoomTask> room_tasks_;
	std::optional<RoomTask> pending_room_task_;

	int room_index_;
	RoomKey room_key_;
	std::string room_name_;
	std::string room_password_;
	std::array<Player, MAX_ROOM_PLAYER_COUNT> room_players_;
	int host_id_ = -1;
	std::atomic<char> current_player_count_{ 0 };

	Session* FindSession(const Player& player) const;
	Player* FindPlayer(SessionKey session_key);
	SessionKey ClearPlayer(Player& player);
	bool InitHostSession(Session& session, SessionKey session_key);
	bool RequestLobbyTransition(SessionKey session_key, RoomExitType exit_type = RoomExitType::LEAVE);
	void CompletePlayerRemoval(SessionKey session_key, RoomExitType exit_type);
	bool IsPlayerInRoom(const Session& session) const;
	void BeginRoomDelete();
	void TryPostRoomDelete();
	void ProcessSessionTasks();
	void ProcessRoomTasks();
	bool TryProcessJoinTask(const RoomTask& task);
	bool TryProcessRoomTask(const RoomTask& task);
	int AddPlayer(Session& new_session, SessionKey session_key, const std::string& room_password);
	bool ProcessSpecificRoomTask(const RoomTask& task);
	void ProcessGameTick(long long tick_time_ms);
	void StartGame(int requester_id);
	void FindNewHost(); // 방장이 나갔을 때 새로운 방장 찾기
	int FindHostIndex(int host_id); // 현재 호스트의 인덱스를 반환
	void TogglePlayerReady(int player_id);
	bool KickPlayer(int requester_id, int kick_player_id);

public:
	GameRoom(GameServer* server, PublicRoomInitData data);
	GameRoom(GameServer* server, PrivateRoomInitData data);
	~GameRoom();

	RoomKey GetRoomKey() const { return room_key_; }
	int GetRoomIndex() const { return room_index_; }
	bool IsPrivate() const { return !room_password_.empty(); }
	int GetMaxPlayerCount() const { return MAX_ROOM_PLAYER_COUNT; }
	int GetCurrentPlayerCount() const;
	RoomInfoSnapshot GetRoomInfoSnapshot();
	const std::string& GetRoomName() const { return room_name_; }
	const std::string& GetRoomPassword() const { return room_password_; }

	// 공통
	void HandlePacket(char* packet, Session& request_session);
	void ProcessRoomTick(long long tick_time_ms);
	void RemovePlayer(SessionKey session_key);
	void SendCreateRoom(Session& session);
	bool AddHostSession(Session& session, SessionKey session_key);
	bool ActivatePlayer(SessionKey session_key);
	bool AddRoomTask(RoomTask task);
	void CompleteRoomInitialization();
	void StoreRoomState(const RoomState new_state);
	bool TryChangeRoomState(RoomState expected, RoomState desired);
	
	void ClearGame();
	
	void ClearRoom();
	void BroadcastPackets();
	void Broadcast(char* packet);
};
