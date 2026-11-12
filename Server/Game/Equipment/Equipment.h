#pragma once
#include <cstdint>
#include "equipment_types.h"

// 매 틱 진행에서 장비에 일어난 일. 방이 알릴 패킷을 고른다.
enum class EquipmentEvent : std::uint8_t { NONE, RELOAD_START, RELOAD_DONE, CHARGE };

// 장착 장비 하나. 플레이어 동작 상태가 종류마다 하나씩 갖는다. 종류마다 정해진 값은 장비 설정 상수를 참조로 읽고,
// 이 장비에서만 바뀌는 상태는 멤버로 든다. 동작 시각은 클라이언트가 보낸 서버 기준 시각(ms), 받은 시각은 서버가 요청을 받은 시각(ms)이다.
class Equipment
{
	const EquipmentSettings& settings_;
	// 이 장비로 바꾼 동작 시각과 교체 대기 중인지. 스폰할 때 손에 든 장비는 대기가 없다.
	std::uint32_t equip_action_ms_ = 0;
	bool has_equip_wait_ = false;

protected:
	explicit Equipment(const EquipmentSettings& settings);
	// 받은 시각 time_ms에 교체 대기가 끝날 때까지 남은 시간(ms). 대기가 없거나 끝났으면 0이다.
	long long GetEquipWaitRemainingMs(long long time_ms) const;

public:
	virtual ~Equipment() = default;

	const EquipmentSettings& GetSettings() const { return settings_; }
	// 교체 대기 중인 동작 시각인지
	bool IsEquipWaiting(std::uint32_t action_time_ms) const;
	// 대기 없이 손에 든 것으로 한다. 스폰할 때 쓴다.
	void ClearEquipWait() { has_equip_wait_ = false; }

	// 게임을 시작할 때 이번 게임의 처음 상태로 되돌린다.
	virtual void Reset() = 0;
	// 동작 시각 기준을 동기화로 보낸 서버 시각으로 맞춘다.
	virtual void ResetActionTime(long long) {}
	// 다른 장비에서 이 장비로 바꾼다. 교체 대기를 시작한다.
	virtual void Equip(std::uint32_t action_time_ms, long long time_ms);
	// 손에서 내려놓는다. 손에 들고 있어야 이어지는 일을 멈춘다.
	virtual void OnUnequip() {}
	// 휘두른다. 교체 대기 중이면 받지 않는다.
	virtual bool TryMelee(std::uint32_t action_time_ms);
	// 지난 시간(ms)만큼 상태를 진행하고 일어난 일을 돌려준다.
	virtual EquipmentEvent Update(long long) { return EquipmentEvent::NONE; }
	virtual bool IsAiming() const { return false; }
	// 조준을 바꾼다. 조준할 수 없는 장비는 조준하려는 요청을 받지 않는다.
	virtual bool TrySetAiming(bool is_aiming) { return !is_aiming; }
};
