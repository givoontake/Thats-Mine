#pragma once
#include <atomic>
#include <cstddef>

struct LobbyPhaseContext
{
	std::atomic<std::size_t> next_lifecycle_task_index{ 0 };
	std::atomic<int> completed_lifecycle_thread_count{ 0 };
	std::atomic<bool> is_matching_complete{ false };
	std::atomic<int> next_lobby_index{ 0 };
	const std::size_t lifecycle_task_count;
	const int thread_count;
	const long long tick_time_ms;

	LobbyPhaseContext(std::size_t lifecycle_task_count, int thread_count, long long tick_time_ms);
};
