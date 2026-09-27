#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <immintrin.h>

// 활성 대상만 순회하기 위한 목록이다. 대상은 고정 배열의 번호(예: 세션 번호)로 나타낸다.
// 목록(list_)과 룩업(lookup_) 두 배열로 관리한다.
//   list_[목록 인덱스]   = 대상 번호 (NONE이면 빈칸). 가장 앞의 빈칸부터 채우고, 삭제해도 다른 대상의 인덱스는 바뀌지 않는다.
//   lookup_[대상 번호]   = 목록 인덱스 / NONE / UPDATING. 빠르게 찾아가기 위한 매핑이며, 유효한 인덱스를 판단하는 기준이다.
// 인덱스가 바뀌지 않으므로 순회 도중 추가·삭제가 일어나도 다른 대상의 순회 위치에 영향을 주지 않는다.
// 여러 스레드가 동시에 추가·삭제·순회해도 안전하다.
template <std::size_t CAPACITY>
class ActiveList
{
public:
	static constexpr std::int32_t NONE = -1;      // 목록에 없음 (list_에서는 빈칸)
	static constexpr std::int32_t UPDATING = -2;  // 이 대상의 목록 인덱스를 고치는 중

	ActiveList()
	{
		for (auto& value : list_) value.store(NONE);
		for (auto& list_index : lookup_) list_index.store(NONE);
	}

	// 대상의 등록 여부를 should_be_listed()의 판단에 맞춘다.
	// 룩업을 UPDATING으로 점유한 뒤 판단을 호출하므로, 같은 대상의 수정은 한 번에 하나씩 일어나고
	// 판단을 대상의 현재 상태로 하면 호출 순서가 뒤섞여도 마지막 상태로 맞춰진다.
	template <typename Predicate>
	void Sync(int value, Predicate should_be_listed)
	{
		if (value < 0 || value >= static_cast<int>(CAPACITY)) return;
		auto& lookup = lookup_[value];

		// 다른 스레드가 같은 대상을 고치는 중이면 끝날 때까지 기다린다.
		std::int32_t current_index = lookup.load();
		for (int spin_count = 0; ; ++spin_count) {
			if (current_index != UPDATING && lookup.compare_exchange_weak(current_index, UPDATING)) break;
			if (spin_count > 64) std::this_thread::yield();
			else _mm_pause();
			current_index = lookup.load();
		}

		bool is_listed = should_be_listed();  // 판단 함수가 항상 같은 값을 반환해도 상수 조건 경고가 나지 않도록 const를 붙이지 않는다.
		std::int32_t new_index = current_index;
		if (is_listed && current_index == NONE) new_index = Add(value);
		else if (!is_listed && current_index != NONE) {
			Remove(current_index);
			new_index = NONE;
		}
		// 인덱스 또는 NONE을 저장해 점유를 풀고 결과를 공개한다.
		lookup.store(new_index);
	}

	// 목록 인덱스에 등록된 유효한 대상 번호를 반환한다. 룩업이 이 인덱스를 가리킬 때만 유효하며,
	// 비워졌거나, 다른 인덱스로 옮겨 갔거나, 수정 중(UPDATING)인 인덱스는 NONE을 반환한다.
	std::int32_t GetValue(int list_index) const
	{
		if (list_index < 0 || list_index >= static_cast<int>(CAPACITY)) return NONE;
		const std::int32_t value = list_[list_index].load();
		if (value < 0 || lookup_[value].load() != list_index) return NONE;
		return value;
	}

	// 순회 경계: 사용 중인 가장 큰 목록 인덱스 + 1
	int GetEnd() const { return end_.load(); }

private:
	std::array<std::atomic<std::int32_t>, CAPACITY> list_;
	std::array<std::atomic<std::int32_t>, CAPACITY> lookup_;
	std::atomic<int> end_{ 0 };

	// 가장 앞의 빈 인덱스를 CAS로 차지한다. 여러 스레드가 동시에 차지해도 한 인덱스는 한 스레드만 얻는다.
	std::int32_t Add(int value)
	{
		for (int list_index = 0; list_index < static_cast<int>(CAPACITY); ++list_index) {
			std::int32_t expected_value = NONE;
			if (!list_[list_index].compare_exchange_strong(expected_value, value)) continue;
			ExtendEnd(list_index);
			return list_index;
		}
		return NONE;
	}

	// 그 인덱스만 비우고 다른 대상의 인덱스는 바꾸지 않는다. 룩업은 호출한 쪽이 이미 점유(UPDATING)해 순회 대상에서 빠진 상태다.
	void Remove(int list_index)
	{
		list_[list_index].store(NONE);
		if (list_index + 1 == end_.load()) ShrinkEnd();
	}

	void ExtendEnd(int list_index)
	{
		const int desired_end = list_index + 1;
		int current_end = end_.load();
		while (current_end < desired_end && !end_.compare_exchange_weak(current_end, desired_end)) {}
	}

	// 뒤쪽의 연속된 빈 인덱스만큼 경계를 당긴다. 당긴 직후 잘라 낸 구간을 다시 확인해,
	// 그사이 다른 스레드가 차지한 인덱스가 있으면 경계를 되돌린다. 차지하는 쪽은 차지한 뒤 경계를 늘리므로,
	// 어느 순서로 겹쳐도 한쪽이 상대의 변경을 보게 되어 사용 중인 인덱스가 경계 밖에 남지 않는다.
	void ShrinkEnd()
	{
		int old_end = end_.load();
		int new_end = old_end;
		while (new_end > 0 && list_[new_end - 1].load() == NONE) --new_end;
		if (new_end == old_end || !end_.compare_exchange_strong(old_end, new_end)) return;
		for (int list_index = new_end; list_index < old_end; ++list_index) {
			if (list_[list_index].load() != NONE) ExtendEnd(list_index);
		}
	}
};
