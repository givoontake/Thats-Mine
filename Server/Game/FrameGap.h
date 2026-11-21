#pragma once
#include <cstdint>

// 한 플레이어의 전송 프레임 번호를 방 틱에 대응시킨다. 방은 틱마다 이 대응으로 그 플레이어의 어느 번호까지 처리할지 정한다.
// 서버 카운트는 스폰한 방 틱부터 센 틱 수이고, 갭은 서버 카운트와 그 틱에 처리할 번호의 차이다.
// 이동 패킷이 번호의 마지막 패킷이므로, 이동 패킷을 받은 번호까지 다 온 것으로 본다.
// 스폰·롤백 확인 뒤 첫 이동 패킷을 받으면, 그 번호를 FRAME_GAP_ALLOWANCE_TICKS 뒤 틱에 처리하도록 갭을 정한다.
// 틱을 시작할 때 다음 번호까지 다 와 있으면 갭을 1 줄여 그 틱에 두 번호를 처리하고,
// 그 틱에 받은 패킷까지 봐도 처리할 번호가 다 오지 않았으면 갭을 1 늘려 그 틱에는 처리하지 않는다.
class FrameGap
{
	std::uint32_t spawn_tick_ = 0;
	long long gap_ = 0;
	bool has_gap_ = false;
	// 이동 패킷까지 받은 마지막 번호. 아직 없으면 -1이다.
	long long complete_number_ = -1;

	long long GetServerCount(std::uint32_t tick) const { return static_cast<long long>(tick - spawn_tick_); }

public:
	// spawn_tick부터 서버 카운트를 센다. 갭은 첫 이동 패킷을 받을 때 정한다.
	void Start(std::uint32_t spawn_tick);
	// 롤백 확인을 받았다. 다음 이동 패킷을 받을 때 갭을 다시 정한다.
	void Restart() { has_gap_ = false; }
	long long GetGap() const { return gap_; }
	// 이 번호가 이동 패킷까지 받은 번호보다 뒤인지. 번호가 줄거나 다 온 번호에 또 온 패킷은 버린다.
	bool IsNewNumber(std::uint32_t frame_number) const { return frame_number > complete_number_; }
	// tick에 받은 이동 패킷의 번호. 그 번호까지 다 왔다. 갭이 없으면 정한다.
	void MarkComplete(std::uint32_t frame_number, std::uint32_t tick);
	// tick을 시작하며 새 패킷을 받기 전에 부른다. 다음 번호까지 다 와 있으면 갭을 1 줄인다.
	void BeginTick(std::uint32_t tick);
	// tick에 받은 패킷까지 넣은 뒤 부른다. 처리할 번호가 다 오지 않았으면 갭을 1 늘린다.
	void SettleTick(std::uint32_t tick);
	// tick에 처리할 마지막 번호. 갭이 없으면 -1이다.
	long long GetProcessNumber(std::uint32_t tick) const { return has_gap_ ? GetServerCount(tick) - gap_ : -1; }
};
