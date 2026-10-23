#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include <array>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include "LatencyRecorder.h"
#include "Session.h"
#include "types.h"

struct ExOverlapped;

// 봇 연결과 시험 흐름을 맡는다.
// 흐름: 연결 → 시험 로그인 → 방장은 방을 만들고, 방마다 두 번째 봇이 로비에서 방 목록으로 그 방 키를 찾음 → 나머지는 그 방에 들어가 준비
//       → 모든 방이 준비되면 방장들이 시작 → 로딩 완료 → 스폰과 플레이 시작을 받으면 걷기 시작
//       → 걸으면서 FIRE_WEAPON으로 바꾸고 FIRE_INTERVAL_MS마다 쏜다 → 모든 봇이 게임 시작을 받으면 MEASUREMENT_SECONDS 동안 잰다.
class TestManager
{
	HANDLE iocp_handle_ = nullptr;
	WSADATA wsa_data_{};
	SOCKADDR_IN server_address_{};
	LPFN_CONNECTEX connect_ex_ = nullptr;
	std::array<std::unique_ptr<Session>, MAX_CLIENT_COUNT> sessions_;
	std::array<std::atomic<RoomKey>, MAX_STRESS_ROOM_COUNT> room_keys_{};
	std::array<std::atomic<long long>, MAX_STRESS_ROOM_COUNT> room_join_ready_times_{};
	std::mutex connect_ex_mutex_;
	std::mutex connect_mutex_;
	std::condition_variable connect_cv_;
	std::mutex log_mutex_;
	std::atomic<int> connected_client_count_{ 0 };
	std::atomic<int> playing_client_count_{ 0 };
	std::atomic<int> game_started_client_count_{ 0 };
	std::atomic<int> ready_room_count_{ 0 };
	std::atomic<int> target_client_count_{ 0 };
	std::atomic<int> started_connect_count_{ 0 };
	std::atomic<int> connect_signal_count_{ 0 };
	std::atomic<bool> is_reconnect_enabled_{ true };
	LatencyRecorder latency_recorder_;

public:
	TestManager(const std::string& server_ip, std::string output_directory);
	~TestManager();

	void Start(int client_count);
	void ProcessConnect();
	void ProcessIO();
	void ProcessSend(int worker_index);
	bool IsMeasurementFinished() const { return latency_recorder_.IsFinished(); }

private:
	long long GetCurrentTimeMs() const;
	bool LoadConnectEx(SOCKET socket);
	bool ConnectToServer();
	int FindAvailableSessionIndex();
	void CompleteConnect(int session_index, ExOverlapped* connect_over);
	void ProcessRecvBuffer(int recv_bytes, int session_index);
	void HandlePacket(char* packet, int session_index);
	void SendTestLogin(int session_index);
	void SendCreateRoom(int host_index);
	void SendRoomListRequest(int session_index, long long now_ms);
	void TryJoinRoom(int session_index);
	void SendWalk(Session& session, long long now_ms);
	void SendFire(Session& session, long long now_ms);
	void NotifyLoginComplete();
	void UpdateMeasurement();
	void Disconnect(int session_index);
};
