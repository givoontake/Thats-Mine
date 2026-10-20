#pragma once
#include <atomic>
#include <cstddef>

struct LobbyPhaseContext
{
	std::atomic<std::size_t> next_request_index{ 0 };
	std::atomic<int> completed_request_thread_count{ 0 };
	std::atomic<bool> is_room_list_claimed{ false };
	const std::size_t request_count;
	const int thread_count;
	const long long tick_time_ms;
	const bool should_refresh_room_list;

	LobbyPhaseContext(std::size_t request_count, int thread_count, long long tick_time_ms, bool should_refresh_room_list);
};
