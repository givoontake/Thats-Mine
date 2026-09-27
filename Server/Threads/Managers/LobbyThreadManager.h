#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include "ActiveList.h"
#include "ConcurrentTaskQueue.h"
#include "enum_class.h"
#include "types.h"
#include "LobbyThread.h"
#include "LoginManager.h"
#include "Matchmaker.h"
#include "lobby_tasks.h"
#include "settings.h"

class GameServer;
class Session;
struct SessionState;

class LobbyThreadManager
{
public:
	static constexpr int THREAD_COUNT = 2;

private:
	GameServer& game_server_;
	std::mutex lobby_mutex_;
	std::condition_variable lobby_cv_;
	ConcurrentTaskQueue<std::unique_ptr<LobbyTask>> lifecycle_tasks_;
	// 로비 목록: 로비가 순회할 세션(방에 있지 않은 활성 세션)의 세션 번호를 담는다. 실제 처리 여부는 세션 담당 상태로 판단한다.
	ActiveList<MAX_PLAYER_COUNT> lobby_list_;
	// 로그인 전 세션의 목록과 로그인 처리. 로그인에 성공한 세션만 로비 목록으로 옮겨 온다.
	LoginManager login_manager_;
	// 빠른 매칭 담당. 로비 단계 안의 매칭 단계에서 한 스레드가 실행한다.
	Matchmaker matchmaker_;
	// 현재 로비 단계의 틱 시각. 로비 단계를 시작할 때 기록하고, 매칭 등록 시각 등에 같은 값을 쓴다.
	std::atomic<long long> tick_time_ms_{ 0 };
	std::vector<std::unique_ptr<LobbyThread>> thread_objects_;
	std::vector<std::thread> threads_;

	friend class LobbyThread;

	void ProcessTask(std::unique_ptr<LobbyTask> task);
	static bool ShouldBeListed(const SessionState& state);
	static bool IsInLobby(ModeState mode_state);
	Session* FindLobbySession(int lobby_index);
	void BroadcastToLobby(char* packet);
	void SendRoomList(Session& session);
	void SendLobbyPlayerList(Session& session);

public:
	explicit LobbyThreadManager(GameServer& game_server);

	void Start();
	void StartLobbyPhase(long long tick_time_ms);
	void Enqueue(std::unique_ptr<LobbyTask> task);
	void SyncLobbyList(int session_index);
	void ProcessPacket(char* packet, Session& session);
	void Close();
	void Join();
};
