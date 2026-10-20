#include "LobbyPhaseContext.h"

LobbyPhaseContext::LobbyPhaseContext(std::size_t lifecycle_task_count, int thread_count, long long tick_time_ms, bool should_refresh_room_list)
	: lifecycle_task_count(lifecycle_task_count), thread_count(thread_count), tick_time_ms(tick_time_ms), should_refresh_room_list(should_refresh_room_list)
{
}
