#include "LobbyPhaseContext.h"

LobbyPhaseContext::LobbyPhaseContext(std::size_t request_count, int thread_count, long long tick_time_ms, bool should_refresh_room_list)
	: request_count(request_count), thread_count(thread_count), tick_time_ms(tick_time_ms), should_refresh_room_list(should_refresh_room_list)
{
}
