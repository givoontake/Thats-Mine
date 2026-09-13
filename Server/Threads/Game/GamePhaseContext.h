#pragma once
#include <atomic>
#include <cstddef>

struct GamePhaseContext
{
	std::atomic<bool> is_lifecycle_preparing{ false };
	std::atomic<bool> is_lifecycle_ready{ false };
	std::atomic<bool> is_lifecycle_complete{ false };
	std::atomic<std::size_t> next_lifecycle_task_index{ 0 };
	std::atomic<int> completed_lifecycle_thread_count{ 0 };
	std::atomic<std::size_t> next_active_room_index{ 0 };
	std::size_t lifecycle_task_count = 0;
	const int thread_count;
    const long long tick_time_ms;

	GamePhaseContext(long long tick_time_ms, int thread_count);
};
