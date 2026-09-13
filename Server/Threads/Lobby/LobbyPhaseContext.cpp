#include "LobbyPhaseContext.h"

LobbyPhaseContext::LobbyPhaseContext(std::size_t lifecycle_task_count, int thread_count)
	: lifecycle_task_count(lifecycle_task_count), thread_count(thread_count)
{
}
