#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include <atomic>
#include <memory>
#include "BoxCandidateData.h"
#include "ExOverlapped.h"
#include "HitboxData.h"
#include "MapRegionData.h"
#include "TerrainData.h"
#include "TowerCandidateData.h"
#include "session_tasks.h"
#pragma comment(lib, "MSWSock.lib")
#pragma comment(lib, "Ws2_32.lib")

class ServerThreadManager;
class SessionManager;
class Session;
class GameRoom;
struct LobbyTask;
struct RoomLifecycleTask;

class GameServer
{
	HANDLE iocp_handle_;
	SOCKET listen_socket_, accept_socket_;
	WSADATA wsa_data_;
	SOCKADDR_IN server_addr_;
	IOOverlapped accept_over_;
	std::unique_ptr<SessionManager> session_manager_;

	std::atomic<bool> is_running_ = true;
	std::unique_ptr<ServerThreadManager> thread_manager_;
	HitboxData hitbox_data_; // 서버 시작 시 읽은 뒤 읽기만 하는 히트박스 판정 데이터
	TerrainData terrain_data_; // 서버 시작 시 읽은 뒤 읽기만 하는 지형 판정 데이터
	BoxCandidateData box_candidate_data_; // 서버 시작 시 읽은 뒤 읽기만 하는 상자 배치 후보
	TowerCandidateData tower_candidate_data_; // 서버 시작 시 읽은 뒤 읽기만 하는 탑 배치 후보
	MapRegionData map_region_data_; // 서버 시작 시 읽은 뒤 읽기만 하는 맵 영역

public:

	GameServer();
	~GameServer();

	bool IsRunning() const { return is_running_.load(); }
	void RequestStop() { is_running_.store(false); }
	HANDLE GetIOCPHandle() const { return iocp_handle_; }
	ServerThreadManager& GetThreadManager() { return *thread_manager_; }
	SessionManager& GetSessionManager() { return *session_manager_; }
	const HitboxData& GetHitboxData() const { return hitbox_data_; }
	const TerrainData& GetTerrainData() const { return terrain_data_; }
	const BoxCandidateData& GetBoxCandidateData() const { return box_candidate_data_; }
	const TowerCandidateData& GetTowerCandidateData() const { return tower_candidate_data_; }
	const MapRegionData& GetMapRegionData() const { return map_region_data_; }

	void SyncLobbyList(int session_index);

	void StartServer();
	void CompleteAccept();
	void StartThreads();
	void CloseThreads();
	void JoinThreads();
	void RoutePacket(char* packet, Session& session, GameRoom* room);
	void ProcessSessionTask(Session& session, std::unique_ptr<SessionTask> task, GameRoom* room);
	SessionDrainResult DrainSessionTasks(Session& session, GameRoom* room);
	void EnqueueLobbyTask(std::unique_ptr<LobbyTask> task);
	void EnqueueRoomLifecycleTask(std::unique_ptr<RoomLifecycleTask> task);
	void SendError(Session& session, int error_code);
};
