#pragma once
#include <array>
#include <cstdint>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include "ActiveList.h"
#include "ConcurrentTaskQueue.h"
#include "IndexRegistry.h"
#include "Lobby/LobbySession.h"
#include "LobbyThread.h"
#include "lobby_tasks.h"

class GameServer;
class Session;

class LobbyThreadManager
{
public:
	static constexpr int THREAD_COUNT = 2;

private:
	GameServer& game_server_;
	std::mutex lobby_mutex_;
	std::condition_variable lobby_cv_;
	ConcurrentTaskQueue<std::unique_ptr<LobbyTask>> lifecycle_tasks_;
	std::array<LobbySession, MAX_PLAYER_COUNT> lobby_sessions_;
	ActiveList<std::uint64_t> active_session_keys_; // 활성 로비 세션만 순회하여 로비 처리 비용을 줄인다.
	IndexRegistry<std::uint64_t> session_index_registry_; // session_key의 session_id로 고정 세션 배열의 session_index를 빠르게 찾는다.
	std::vector<std::unique_ptr<LobbyThread>> thread_objects_;
	std::vector<std::thread> threads_;

	friend class LobbyThread;

	void ProcessTask(std::unique_ptr<LobbyTask> task);
	void FindMatch(SessionKey session_key, int max_player_count);
	void BroadcastToLobby(char* packet);
	void SendRoomList(Session& session);
	void SendLobbyPlayerList(Session& session);
	bool AddActiveSession(SessionKey session_key);
	Session* GetActiveSession(std::size_t active_session_index);

public:
	explicit LobbyThreadManager(GameServer& game_server);

	void Start();
	void StartLobbyPhase();
	void Enqueue(std::unique_ptr<LobbyTask> task);
	LobbySession* GetLobbySession(int session_index);
	void RemoveActiveSession(SessionKey session_key);
	bool BeginRoomTransition(SessionKey session_key);
	void ProcessPacket(char* packet, Session& session);
	void Close();
	void Join();
};
