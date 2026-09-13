#include "GamePhaseContext.h"

GamePhaseContext::GamePhaseContext(long long tick_time_ms, int thread_count)
	: thread_count(thread_count), tick_time_ms(tick_time_ms)
{
}
