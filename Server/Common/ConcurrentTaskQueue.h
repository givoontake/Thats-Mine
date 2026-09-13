#pragma once
#include <atomic>
#include <cstddef>
#include <utility>
#include <immintrin.h>
#include <oneapi/tbb/concurrent_queue.h>

template <typename T>
class ConcurrentTaskQueue
{
	oneapi::tbb::concurrent_queue<T> task_queue_;
	std::atomic<std::size_t> task_count_{ 0 };

public:
	void Enqueue(T task)
	{
		task_count_.fetch_add(1);
		task_queue_.push(std::move(task));
	}

	std::size_t ClaimTaskCount()
	{
		return task_count_.exchange(0);
	}

	void RestoreClaimedTaskCount(std::size_t task_count)
	{
		if (task_count == 0) return;
		task_count_.fetch_add(task_count);
	}

	std::size_t GetTaskCount() const
	{
		return task_count_.load();
	}

	T Dequeue()
	{
		T task;
		while (!task_queue_.try_pop(task)) _mm_pause();
		return task;
	}
};
