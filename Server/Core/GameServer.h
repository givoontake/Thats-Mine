#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include <array>
#include <atomic>
#include <memory>
#include "BoxCandidateData.h"
#include "BoxShapeData.h"
#include "DirectionHintData.h"
#include "ExOverlapped.h"
#include "HintObjectTree.h"
#include "HitboxData.h"
#include "MapData.h"
#include "MapRegionData.h"
#include "session_tasks.h"
#pragma comment(lib, "MSWSock.lib")
#pragma comment(lib, "Ws2_32.lib")

class ServerThreadManager;
class SessionManager;
class Session;
class GameRoom;
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
	// 자원 부족 등으로 다시 걸지 못한 수락 요청 자리. 타이머가 넣은 완료를 받은 입출력 스레드가 다시 건다.
	std::array<std::atomic<bool>, ACCEPT_COUNT> is_accept_retry_needed_{};
	ExOverlapped accept_retry_over_;
	std::atomic<bool> is_accept_retry_posted_ = false;
	std::unique_ptr<SessionManager> session_manager_;
	// 타이머가 연결 종료 정리를 입출력 스레드에 맡길 때 넣는 완료. 정리가 끝나기 전에는 다시 넣지 않아, 정리 단계는 한 번에 하나만 돈다.
	ExOverlapped disconnect_cleanup_over_;
	std::atomic<bool> is_disconnect_cleanup_posted_ = false;

	std::atomic<bool> is_running_ = true;
	std::unique_ptr<ServerThreadManager> thread_manager_;
	HitboxData hitbox_data_; // 서버 시작 시 읽은 뒤 읽기만 하는 히트박스 판정 데이터
	MapData map_data_; // 서버 시작 시 읽은 뒤 읽기만 하는 맵 데이터(랜드스케이프, 충돌 배치물)
	BoxCandidateData box_candidate_data_; // 서버 시작 시 읽은 뒤 읽기만 하는 상자 배치 후보
	BoxShapeData box_shape_data_; // 서버 시작 시 읽은 뒤 읽기만 하는 상자 충돌 모양
	MapRegionData map_region_data_; // 서버 시작 시 읽은 뒤 읽기만 하는 맵 영역
	HintObjectTree hint_object_tree_; // 서버 시작 시 맵 데이터로 만든 뒤 읽기만 하는 힌트 물체 트리
	DirectionHintData direction_hint_data_; // 서버 시작 시 맵 영역으로 만든 뒤 읽기만 하는 방향 힌트 기준

	void PostAccept(AcceptOverlapped& accept_over);
	void MarkAcceptRetry(AcceptOverlapped& accept_over);

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
	const BoxShapeData& GetBoxShapeData() const { return box_shape_data_; }
	const MapRegionData& GetMapRegionData() const { return map_region_data_; }
	const HintObjectTree& GetHintObjectTree() const { return hint_object_tree_; }
	const DirectionHintData& GetDirectionHintData() const { return direction_hint_data_; }

	void SyncLoginList(int session_index);
	void SyncLobbyList(int session_index);

	void StartServer();
	void CompleteAccept(AcceptOverlapped& accept_over);
	void FailAccept(AcceptOverlapped& accept_over, DWORD error);
	void PostAcceptRetry();
	void RetryFailedAccepts();
	void StartThreads();
	void CloseThreads();
	void JoinThreads();
	bool RoutePacket(Session& session, std::unique_ptr<SessionPacketTask> task);
	void RequestDisconnect(SessionKey session_key);
	void PostDisconnectCleanup();
	void ProcessDisconnects();
	void EnqueueRoomLifecycleTask(std::unique_ptr<RoomLifecycleTask> task);
	void SendError(Session& session, int error_code);
};
