#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include "SendBuffer.h"
#include "enum_class.h"
#include "db_login_result.h"

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
	// 세션 키를 이루는 값. 여러 스레드가 언제든 세션 키를 비교하므로 원자 변수로 둔다.
	// 세션 번호는 배열 위치라 바뀌지 않고, 세션 아이디는 접속 배정·해제 때, 플레이어 번호는 로그인 성공·해제 때만 바뀐다.
	std::atomic<int> session_index_ = -1;
	std::atomic<int> player_id_ = -1;
	std::atomic<std::uint64_t> session_id_ = 0;
	// 플레이어 이름. 로그인 DB 스레드가 로그인 대기 소속일 때 적고, 다음 소속은 넘겨받은 뒤에 읽으며, 해제 때 지운다.
	// 소속을 넘기는 상태 비교·교체를 사이에 두고 쓰기와 읽기가 갈리므로 겹치지 않는다.
	std::string nickname_;
	// 수신 완료를 처리하는 입출력 스레드만 쓴다. 수신은 세션마다 하나만 걸고, 접속 배정·해제는 남은 입출력이 없을 때만 한다.
	int remaining_data_size_ = 0;
	std::atomic<int> io_pending_count_ = 0;
	// 들어가 있거나 들어가려는 방 번호. 방으로 옮기는 요청을 보내는 쪽이 방 전환 대기로 바꾸기 전에 적는다.
	// 입장 요청이면 로비·매칭이 목적지 방을, 생성 요청이면 로비가 -1(아직 방 없음)을 적고, 생성 처리가 방을 연 뒤 확정하기 전에 그 방을 적는다.
	// 입출력 스레드가 방 소속·방 전환 대기 세션의 패킷을 보낼 곳을 찾을 때 읽는다. 잘못 읽어도 방이 세션 키로 자기 플레이어인지 다시 확인한다.
	std::atomic<int> room_index_ = -1;
	// 지금 들어가 있는 방의 방 키. 방 번호와 함께 적고, 입출력 스레드가 받은 패킷에 담는다. 방은 꺼낼 때 자기 방 키와 비교해 같은 자리에 예전에 있던 방의 패킷이면 버린다.
	std::atomic<RoomKey> room_key_ = 0;
	// 남은 데이터는 recv_over 버퍼에 들어 있으므로 추가로 만들 필요가 없음.

	// 송수신 등록은 공유 잠금으로 병렬 처리하고 closesocket은 단독 잠금으로 막아, 닫힌 소켓 번호가 새 연결에 재사용되는 경쟁을 방지한다.
	mutable std::shared_mutex socket_mutex_;
	// Microsoft 문서는 스트림 소켓에 여러 스레드가 동시에 송신 요청을 보내면 일부 제공자에서 데이터가 섞일 수 있다고 경고한다.
	// 송신 등록을 따로 묶지 않는 것은 세션마다 그 순간 다루는 스레드가 하나뿐이기 때문이다.
	// 로그인·매칭은 스레드 하나, 로비는 같은 틱에 세션마다 요청 하나, 방은 처리권을 얻은 스레드 하나가 다루고, 소속을 넘길 때는 넘기기 전과 받은 뒤로 송신이 갈린다.
	std::atomic<SessionState> state_{};


public:
	Session();

	bool InitSession(int session_index, std::uint64_t session_id, SOCKET new_socket, HANDLE iocp_handle);
	void SetPlayerInfo(const DBLoginResult& result);
	std::string GetNickname() const;
	bool SendPacket(const char* packet, int packet_size);
	// BUF_SIZE보다 큰 데이터를 크기에 맞춘 송신 버퍼를 새로 만들어 보낸다. 패킷 여러 개를 이어 붙인 데이터도 되고, 패킷 하나의 크기는 만드는 쪽이 지킨다.
	bool SendLargePacket(const char* packet, int packet_size);
	bool RecvPacket();
	ModeState BeginDisconnect(SessionKey session_key);
	void CompleteIO(SessionKey session_key);
	bool SetDisconnectReady(ModeState expected_mode);
	void CloseSocket();
	bool TryRelease();

	// 데이터 레이스를 방지하기 위해 세션은 언제든지 수정될 수 있는 데이터에 대해 참조 반환을 하지 않는다
	//getters
	IOOverlapped& GetRecvOver() { return recv_over_; }
	SessionKey GetSessionKey() const;
	int GetPlayerId() const { return player_id_.load(); }
	int GetRemainingDataSize() const { return remaining_data_size_; }
	SessionState GetState() const { return state_.load(); }
	LifeState GetLifeState() const { return state_.load().life; }
	ModeState GetModeState() const { return state_.load().mode; }
	int GetRoomIndex() const { return room_index_.load(); }
	RoomKey GetRoomKey() const { return room_key_.load(); }

	//setters
	bool MatchesSessionKey(SessionKey session_key) const;
	void AdjustRemainingDataSize(int data_size_delta);
	void SetRoom(int room_index, RoomKey room_key)
	{
		room_index_.store(room_index);
		room_key_.store(room_key);
	}
	bool TryChangeMode(ModeState expected_mode, ModeState desired_mode);
};
