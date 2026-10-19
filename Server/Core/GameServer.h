#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include <array>
#include <atomic>
#include <memory>
#include "BoxCandidateData.h"
#include "DirectionHintData.h"
#include "ExOverlapped.h"
#include "HintObjectTree.h"
#include "HitboxData.h"
#include "MapData.h"
#include "MapRegionData.h"
#include "TowerCandidateData.h"
#include "ViewHintData.h"
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
	// 동시에 걸어 두는 접속 수락 요청 수. 하나가 처리되는 동안에도 나머지가 접속을 받는다.
	static constexpr int ACCEPT_COUNT = 10;

	HANDLE iocp_handle_;
	SOCKET listen_socket_;
	WSADATA wsa_data_;
	SOCKADDR_IN server_addr_;
	// 수락 요청마다 자기 소켓과 OVERLAPPED를 가진다. 완료된 요청은 같은 자리에서 다시 건다.
	std::array<AcceptOverlapped, ACCEPT_COUNT> accept_overs_;
	std::unique_ptr<SessionManager> session_manager_;

	std::atomic<bool> is_running_ = true;
	std::unique_ptr<ServerThreadManager> thread_manager_;
	HitboxData hitbox_data_; // 서버 시작 시 읽은 뒤 읽기만 하는 히트박스 판정 데이터
	MapData map_data_; // 서버 시작 시 읽은 뒤 읽기만 하는 맵 데이터(랜드스케이프, 충돌 배치물)
	BoxCandidateData box_candidate_data_; // 서버 시작 시 읽은 뒤 읽기만 하는 상자 배치 후보
	TowerCandidateData tower_candidate_data_; // 서버 시작 시 읽은 뒤 읽기만 하는 탑 배치 후보
	MapRegionData map_region_data_; // 서버 시작 시 읽은 뒤 읽기만 하는 맵 영역
	HintObjectTree hint_object_tree_; // 서버 시작 시 맵 데이터로 만든 뒤 읽기만 하는 힌트 물체 트리
	DirectionHintData direction_hint_data_; // 서버 시작 시 맵 영역으로 만든 뒤 읽기만 하는 방향 힌트 기준
	ViewHintData view_hint_data_; // 서버 시작 시 읽은 뒤 읽기만 하는 보임 힌트 판정 위치

	void PostAccept(AcceptOverlapped& accept_over);

public:

	GameServer();
	~GameServer();

	bool IsRunning() const { return is_running_.load(); }
	void RequestStop() { is_running_.store(false); }
	HANDLE GetIOCPHandle() const { return iocp_handle_; }
	ServerThreadManager& GetThreadManager() { return *thread_manager_; }
	SessionManager& GetSessionManager() { return *session_manager_; }
	const HitboxData& GetHitboxData() const { return hitbox_data_; }
	const MapData& GetMapData() const { return map_data_; }
	const BoxCandidateData& GetBoxCandidateData() const { return box_candidate_data_; }
	const TowerCandidateData& GetTowerCandidateData() const { return tower_candidate_data_; }
	const MapRegionData& GetMapRegionData() const { return map_region_data_; }
	const HintObjectTree& GetHintObjectTree() const { return hint_object_tree_; }
	const DirectionHintData& GetDirectionHintData() const { return direction_hint_data_; }
	const ViewHintData& GetViewHintData() const { return view_hint_data_; }

	void SyncLobbyList(int session_index);

	void StartServer();
	void CompleteAccept(AcceptOverlapped& accept_over);
	void StartThreads();
	void CloseThreads();
	void JoinThreads();
	void RoutePacket(char* packet, Session& session, GameRoom* room, long long receive_time_ms);
	void ProcessSessionTask(Session& session, std::unique_ptr<SessionTask> task, GameRoom* room);
	SessionDrainResult DrainSessionTasks(Session& session, GameRoom* room);
	void EnqueueLobbyTask(std::unique_ptr<LobbyTask> task);
	void EnqueueRoomLifecycleTask(std::unique_ptr<RoomLifecycleTask> task);
	void SendError(Session& session, int error_code);
};
