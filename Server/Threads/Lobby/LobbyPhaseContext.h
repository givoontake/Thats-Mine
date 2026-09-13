#pragma once
#include <atomic>
#include <cstddef>

struct LobbyPhaseContext
{
	std::atomic<std::size_t> next_lifecycle_task_index{ 0 };
	std::atomic<int> completed_lifecycle_thread_count{ 0 };
	std::atomic<std::size_t> next_active_session_index{ 0 };
	const std::size_t lifecycle_task_count;
	const int thread_count;

	LobbyPhaseContext(std::size_t lifecycle_task_count, int thread_count);
};
