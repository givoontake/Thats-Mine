#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include <array>
#include <atomic>
#include <memory>
#include "types.h"
#include "ExOverlapped.h"
#include "ActivePlayerManager.h"
#include "Session.h"
#include "GameRoom.h"
#pragma comment(lib, "MSWSock.lib")
#pragma comment(lib, "Ws2_32.lib")

class ServerThreadManager;
struct LobbyTask;
struct RoomLifecycleTask;
struct SessionTask;

class GameServer
{
	HANDLE iocp_handle_;
	SOCKET listen_socket_, accept_socket_;
	WSADATA wsa_data_;
	SOCKADDR_IN server_addr_;
	IOOverlapped accept_over_;
	ActivePlayerManager active_players_;
	std::array<Session, MAX_PLAYER_COUNT> sessions_;
	// 방은 미리 만들어 둔 고정 배열의 자리를 재사용한다. 자리 재사용은 방 키(생성마다 새 난수)로 구분한다.
	std::array<GameRoom, MAX_ROOM_COUNT> rooms_;

	std::atomic<bool> is_running_ = true;
	std::unique_ptr<ServerThreadManager> thread_manager_;

public:

	GameServer();
	~GameServer();

	Session* AcquireSession(SOCKET new_socket);
	std::uint64_t GenerateSessionID();
	bool IsRunning() const { return is_running_.load(); }
	void RequestStop() { is_running_.store(false); }
	HANDLE GetIOCPHandle() const { return iocp_handle_; }
	ServerThreadManager& GetThreadManager() { return *thread_manager_; }

	ActivePlayerManager& GetActivePlayerManager() { return active_players_; }

	Session* FindSessionByIndex(int session_index);
	void SyncLobbyList(int session_index);
	Session* FindSession(SessionKey session_key);
	GameRoom* GetRoomByIndex(int room_index);

	void RequestDisconnect(SessionKey session_key);
	void CompleteSessionIO(SessionKey session_key);
	void ReleaseOwnership(Session& session);
	void TryReleaseSession(Session& session);
	void StartServer();
	void CompleteAccept();
	void StartThreads();
	void CloseThreads();
	void JoinThreads();
	void RoutePacket(char* packet, Session& session, GameRoom* room);
	void ProcessSessionTask(Session& session, std::unique_ptr<SessionTask> task, GameRoom* room);
	SessionDrainResult DrainSessionTasks(Session& session, GameRoom* room);
	bool EnqueueSessionTask(SessionKey session_key, std::unique_ptr<SessionTask> task);
	void EnqueueLobbyTask(std::unique_ptr<LobbyTask> task);
	void EnqueueRoomLifecycleTask(std::unique_ptr<RoomLifecycleTask> task);
	void StringToCharBuf(const std::string& str, char* buf, int buf_size);
	std::string CharBufToString(const char* buf, int buf_size);
	void CopyCharBuf(char* dst, int dst_size, const char* src, int src_size);
	void SendError(Session& session, int error_code);
};
