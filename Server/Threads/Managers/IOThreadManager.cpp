#include "IOThreadManager.h"
#include "GameServer.h"

IOThreadManager::IOThreadManager(GameServer& game_server)
    : game_server_(game_server)
{
}

void IOThreadManager::Start(int thread_count)
{
    if (thread_count <= 0) return;
    thread_objects_.reserve(thread_count);
    threads_.reserve(thread_count);
    for (int i = 0; i < thread_count; ++i) {
        thread_objects_.emplace_back(std::make_unique<IOThread>(game_server_));
        thread_objects_.back()->Start();
        threads_.emplace_back(&ServerThread::Run, thread_objects_.back().get());
    }
}

void IOThreadManager::Close()
{
    for (auto& thread_object : thread_objects_)
        thread_object->Close();
}

void IOThreadManager::Join()
{
    for (auto& thread : threads_)
        thread.join();
}
