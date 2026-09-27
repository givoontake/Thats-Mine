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

// 연결 상태와 담당 상태를 한 번에 읽고 비교·교체하기 위해 원자 변수 하나에 함께 저장한다.
struct SessionState
{
	LifeState life = LifeState::NONE;
	ModeState mode = ModeState::NONE;
};
static_assert(sizeof(SessionState) == 2);
static_assert(std::atomic<SessionState>::is_always_lock_free);

class Session
{
	SOCKET socket_ = INVALID_SOCKET;
	HANDLE iocp_handle_ = nullptr; // GameServer가 소유하며 Session은 비소유로 사용한다.
	IOOverlapped recv_over_;
	SendBufferPool send_buffer_pool_;
	SessionKey session_key_;
	std::string nickname_;
	int remaining_data_size_ = 0;
	std::atomic<int> io_pending_count_ = 0;
	// 작업 큐는 세션이 소유하고, 현재 담당자(로비 또는 방)만 소비한다. 전환 대기 중에는 아무도 소비하지 않고 작업을 보존한다.
	ConcurrentTaskQueue<std::unique_ptr<SessionTask>> session_tasks_;
	// 남은 데이터는 recv_over 버퍼에 들어 있으므로 추가로 만들 필요가 없음.

	// 송수신 등록은 공유 잠금으로 병렬 처리하고 closesocket은 단독 잠금으로 막아, 닫힌 소켓 번호가 새 연결에 재사용되는 경쟁을 방지한다.
	mutable std::shared_mutex socket_mutex_;
	// 같은 소켓에 여러 스레드가 동시에 WSASend를 호출하지 않도록 송신 등록만 세션 단위로 직렬화한다.
	// Microsoft 문서는 스트림 소켓에 동시 송신 요청을 보내면 일부 제공자에서 데이터가 섞일 수 있다고 경고한다.
	// 로비 방송과 담당자의 응답 송신이 같은 세션에 동시에 일어날 수 있어 필요하다.
	std::mutex send_mutex_;
	mutable std::mutex session_mutex_;
	std::atomic<SessionState> state_{};


public:
	Session();

	bool InitSession(int session_index, std::uint64_t session_id, SOCKET new_socket, HANDLE iocp_handle);
	bool ApplyLoginResult(const DBLoginResult& result);
	std::string GetNickname() const;
	bool SendPacket(const char* packet, int packet_size);
	bool RecvPacket();
	bool BeginDisconnect(SessionKey session_key);
	bool CompleteIO(SessionKey session_key);
	void ReleaseMode();
	bool TryBeginRelease();
	void CompleteRelease();
	bool TryEnqueueTask(SessionKey session_key, std::unique_ptr<SessionTask> task);
	void DiscardTasks();
	std::size_t ClaimTaskCount();
	void RestoreClaimedTaskCount(std::size_t task_count);
	std::unique_ptr<SessionTask> DequeueTask();

	// 데이터 레이스를 방지하기 위해 세션은 언제든지 수정될 수 있는 데이터에 대해 참조 반환을 하지 않는다
	//getters
	IOOverlapped& GetRecvOver() { return recv_over_; }
	SessionKey GetSessionKey() const;
	int GetRemainingDataSize() const;
	SessionState GetState() const { return state_.load(); }
	LifeState GetLifeState() const { return state_.load().life; }
	ModeState GetModeState() const { return state_.load().mode; }

	//setters
	bool MatchesSessionKey(SessionKey session_key) const;
	void AdjustRemainingDataSize(int data_size_delta);
	bool TryChangeMode(ModeState expected_mode, ModeState desired_mode);
};
