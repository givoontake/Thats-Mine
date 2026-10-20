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
#include "Matchmaker.h"
#include "session_tasks.h"
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
	// 로비 요청 큐: 입출력 스레드가 로비 소속 세션의 패킷을 넣는다. 요청 단계에서 로비 스레드들이 나눠 처리한다.
	ConcurrentTaskQueue<std::unique_ptr<SessionPacketTask>> request_tasks_;
	// 로비 목록: 로그인했고 방에 있지 않은 활성 세션의 세션 번호. 로비 방송 대상이다.
	ActiveList<MAX_PLAYER_COUNT> lobby_list_;
	// 빠른 매칭 처리. 요청 단계가 끝난 뒤 한 스레드가 실행한다.
	Matchmaker matchmaker_;
	// 현재 로비 단계의 틱 시각. 로비 단계를 시작할 때 기록하고, 매칭 등록 시각 등에 같은 값을 쓴다.
	std::atomic<long long> tick_time_ms_{ 0 };
	// 방 목록 응답. 갱신할 틱에 로비 스레드 하나가 새 목록을 만들어 통째로 교체하므로, 요청을 처리하는 쪽은 언제나 완성된 목록을 받는다.
	std::atomic<std::shared_ptr<const std::vector<char>>> room_list_;
	// 마지막으로 방 목록 갱신을 정한 틱 시각. 로비 단계를 시작하는 타이머 스레드만 쓴다.
	long long room_list_refresh_time_ms_ = 0;
	std::vector<std::unique_ptr<LobbyThread>> thread_objects_;
	std::vector<std::thread> threads_;

	friend class LobbyThread;

	void ProcessRequest(std::unique_ptr<SessionPacketTask> task);
	static bool ShouldBeListed(const SessionState& state);
	void RefreshRoomList();
	void SendRoomList(Session& session);

public:
	explicit LobbyThreadManager(GameServer& game_server);

	void Start();
	void StartLobbyPhase(long long tick_time_ms);
	void EnqueueRequest(std::unique_ptr<SessionPacketTask> task);
	void SyncLobbyList(int session_index);
	void ProcessPacket(char* packet, Session& session);
	Matchmaker& GetMatchmaker() { return matchmaker_; }
	void Close();
	void Join();
};
