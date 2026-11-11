#include "LobbyPhaseContext.h"

LobbyPhaseContext::LobbyPhaseContext(std::size_t lobby_task_count, std::size_t request_count, std::uint64_t tick_number, bool should_refresh_room_list)
	: lobby_task_count(lobby_task_count), request_count(request_count), tick_number(tick_number), should_refresh_room_list(should_refresh_room_list)
{
}
