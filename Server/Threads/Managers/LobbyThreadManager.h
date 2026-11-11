#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include "ConcurrentTaskQueue.h"
#include "enum_class.h"
#include "types.h"
#include "LobbyThread.h"
#include "lobby_task.h"
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
	// 로비 작업 큐: 다른 처리 단위가 로비로 옮기는 요청, 연결 종료 정리 통보를 넣는다. 요청 처리 전에 로비 스레드 하나가 처리한다.
	ConcurrentTaskQueue<LobbyTask> lobby_tasks_;
	// 로비 요청 큐: 입출력 스레드가 로비 소속 세션의 패킷을 넣는다. 요청 단계에서 로비 스레드들이 나눠 처리한다.
	ConcurrentTaskQueue<std::unique_ptr<SessionPacketTask>> request_tasks_;
	// 세션 번호별로 마지막으로 요청을 처리한 틱 번호. 요청 처리 스레드들이 처리 전에 이번 틱 번호로 바꾼다.
	// 로비는 순회할 일이 없고 로비 소속은 세션 상태로 판단하므로, 세션 번호로 바로 찾는 칸에 처리 틱만 둔다.
	std::array<std::atomic<std::uint64_t>, MAX_PLAYER_COUNT> last_request_ticks_{};
	// 방 목록 응답. 갱신할 틱에 로비 스레드 하나가 새 목록을 만들어 통째로 교체하므로, 요청을 처리하는 쪽은 언제나 완성된 목록을 받는다.
	std::atomic<std::shared_ptr<const std::vector<char>>> room_list_;
	// 마지막으로 방 목록 갱신을 정한 틱 시각. 로비 단계를 시작하는 타이머 스레드만 쓴다.
	long long room_list_refresh_time_ms_ = 0;
	std::vector<std::unique_ptr<LobbyThread>> thread_objects_;
	std::vector<std::thread> threads_;

	friend class LobbyThread;

	void ProcessTask(LobbyTask task);
	void ConfirmEnter(Session& session, const LobbyTask& task);
	void JoinRoom(Session& session, RoomKey room_key, const char* room_password);
	void ProcessRequest(std::unique_ptr<SessionPacketTask> task, std::uint64_t tick_number);
	static bool IsLobbySession(const SessionState& state);
	static bool IsLobbyRequest(std::uint8_t packet_type);
	void RefreshRoomList();
	void SendRoomList(Session& session);

public:
	explicit LobbyThreadManager(GameServer& game_server);

	void Start();
	void StartLobbyPhase(long long tick_time_ms, std::uint64_t tick_number);
	void EnqueueTask(LobbyTask task);
	void ReturnToLobby(Session& session, ModeState pending_mode, int error_code);
	void EnqueueRequest(std::unique_ptr<SessionPacketTask> task);
	void ProcessPacket(char* packet, Session& session);
	void Close();
	void Join();
};
