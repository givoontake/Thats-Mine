#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include "SendBuffer.h"
#include "ConcurrentTaskQueue.h"
#include "enum_class.h"
#include "session_tasks.h"

// 비동기 송수신 완료가 세션 슬롯 재사용 이후 도착할 수 있으므로 등록 시 세션 식별자를 오버랩 구조체에 저장하고, 완료 시 현재 식별자와 비교한다.
// 송수신 등록과 소켓 종료의 경쟁은 소켓 공유 잠금으로 방지한다.

struct RoomSnapshot {
	ModeState mode_state;
	int room_index;
};

class Session
{
	SOCKET socket_ = INVALID_SOCKET;
	HANDLE iocp_handle_ = nullptr; // GameServer가 소유하며 Session은 비소유로 사용한다.
	IOOverlapped recv_over_;
	SendBufferPool send_buffer_pool_;
	SessionKey session_key_;
	std::string nickname_;
	int room_index_ = -1;
	int remaining_data_size_ = 0;
	std::atomic<int> io_pending_count_ = 0;
	ConcurrentTaskQueue<std::unique_ptr<SessionTask>> session_tasks_;
	// 로비와 방 단계가 동시에 실행돼도 한 세션의 작업 큐는 한 스레드만 소비하도록 한다.
	std::atomic<bool> is_processing_tasks_{ false };
	// 남은 데이터는 recv_over 버퍼에 들어 있으므로 추가로 만들 필요가 없음.

	// 송수신 등록은 공유 잠금으로 병렬 처리하고 closesocket은 단독 잠금으로 막아, 닫힌 소켓 번호가 새 연결에 재사용되는 경쟁을 방지한다.
	mutable std::shared_mutex socket_mutex_;
	mutable std::mutex session_mutex_;
	std::atomic<LifeState> life_state_ = LifeState::NONE;
	std::atomic<ModeState> mode_state_ = ModeState::NONE;


public:
	Session();

	bool InitSession(int session_index, std::uint64_t session_id, SOCKET new_socket, HANDLE iocp_handle);
	bool ApplyLoginResult(const DBLoginResult& result);
	std::string GetNickname() const;
	bool SendPacket(const char* packet, int packet_size);
	bool RecvPacket();
	bool BeginDisconnect(SessionKey session_key);
	bool CompleteIO(SessionKey session_key);
	bool CompleteRoomRemoval(SessionKey session_key);
	void FinalizeDisconnect(SessionKey session_key);
	bool TryEnqueueTask(SessionKey session_key, std::unique_ptr<SessionTask> task);
	void DiscardTasks();
	std::size_t ClaimTaskCount();
	void RestoreClaimedTaskCount(std::size_t task_count);
	std::unique_ptr<SessionTask> DequeueTask();
	bool TryStartTaskProcessing();
	void CompleteTaskProcessing();

	// 데이터 레이스를 방지하기 위해 세션은 언제든지 수정될 수 있는 데이터에 대해 참조 반환을 하지 않는다
	//getters
	IOOverlapped& GetRecvOver() { return recv_over_; }
	SessionKey GetSessionKey() const;
	int GetRoomIndex() const;
	RoomSnapshot GetRoomSnapshot() const;
	int GetRemainingDataSize() const;
	LifeState GetLifeState() const { return life_state_.load(); }
	ModeState GetModeState() const { return mode_state_.load(); }

	//setters
	bool MatchesSessionKey(SessionKey session_key) const;
	void AdjustRemainingDataSize(int data_size_delta);
	bool TrySetLobbyMode(SessionKey session_key);
	bool TrySetRoomMode(SessionKey session_key, int new_room_index);
};
