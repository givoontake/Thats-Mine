#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>

struct LobbyPhaseContext
{
	std::atomic<bool> is_lobby_task_claimed{ false };
	std::atomic<bool> is_lobby_task_done{ false };
	std::atomic<std::size_t> next_request_index{ 0 };
	std::atomic<bool> is_room_list_claimed{ false };
	const std::size_t lobby_task_count;
	const std::size_t request_count;
	const std::uint64_t tick_number;
	const bool should_refresh_room_list;

	LobbyPhaseContext(std::size_t lobby_task_count, std::size_t request_count, std::uint64_t tick_number, bool should_refresh_room_list);
};
