#include <chrono>
#include <immintrin.h>
#include "TimerThread.h"
#include "ServerThreadManager.h"
#include "GameServer.h"

TimerThread::TimerThread(ServerThreadManager& manager) : manager_(manager)
{
}

void TimerThread::Run()
{
    // 시스템 날짜나 시간이 변경되어도 틱 간격 계산에 영향을 받지 않는 시계를 사용한다.
    using Clock = std::chrono::steady_clock;

    const auto frame_time = std::chrono::milliseconds(TICK_INTERVAL_MS);
    // 현재는 시각을 공유하지 않고, 타이머 실행 시점부터 한 주기 뒤를 첫 틱으로 예약한다.
    auto next_tick = Clock::now() + frame_time;
    // 0x00000002는 고해상도 타이머 생성 옵션이다. 타이머는 한 번 만들고 매 틱 재설정하여 사용한다.
    constexpr DWORD HIGH_RESOLUTION_WAITABLE_TIMER_FLAG = 0x00000002;
    HANDLE high_resolution_timer = CreateWaitableTimerExW(nullptr, nullptr, HIGH_RESOLUTION_WAITABLE_TIMER_FLAG, TIMER_MODIFY_STATE | SYNCHRONIZE);
    if (!high_resolution_timer) {
        manager_.CloseThreads();
        return;
    }

    while (is_running_.load() && manager_.game_server_.IsRunning())
    {
        bool is_timer_wait_succeeded = true;
        while (is_running_.load() && manager_.game_server_.IsRunning()) {
            const auto current_time = Clock::now();
            // 이미 예약 시각이 지났다면 더 잠들지 않고 작업 스레드 확보 단계로 넘어간다.
            if (current_time >= next_tick) break;
            const auto remaining_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(next_tick - current_time).count();
            LARGE_INTEGER due_time{};
			// 음수는 현재부터 기다릴 상대 시간을 뜻한다. SetWaitableTimer가 100나노초 단위라 기존 ns을 100ns로 만들어야 하므로 100으로 나눈다.
            due_time.QuadPart = -(remaining_ns / 100);
            // 반복 주기 0으로 한 번만 만료되도록 설정하고, 타이머가 신호 상태가 될 때까지 잠든다.
            if (!SetWaitableTimer(high_resolution_timer, &due_time, 0, nullptr, nullptr, FALSE) || WaitForSingleObject(high_resolution_timer, INFINITE) != WAIT_OBJECT_0) {
                is_timer_wait_succeeded = false;
                break;
            }
        }
        if (!is_timer_wait_succeeded) {
            // 단순한 기상 지연이 아니라 타이머 설정 또는 대기 함수가 실패한 경우에만 종료한다.
            manager_.CloseThreads();
            break;
        }
        if (!is_running_.load() || !manager_.game_server_.IsRunning()) break;

        // 틱 시각이 되어도 사용 가능한 작업 스레드가 최소 개수 이상 있어야 새 틱을 시작한다.
        // 여기서는 잠들지 않고 완료 여부를 반복 확인한다.
        while (is_running_.load() && manager_.game_server_.IsRunning()) {
            if (manager_.game_thread_manager_.GetAvailableThreadCount() >= GameThreadManager::MIN_AVAILABLE_THREAD_COUNT) break;
            _mm_pause();
        }
        if (!is_running_.load() || !manager_.game_server_.IsRunning()) break;

        const auto tick_start_time = Clock::now();
        const long long tick_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(tick_start_time.time_since_epoch()).count();
        // 실제로 깨어난 시각이 아닌 이전 예약 시각에 한 주기를 더하여 기본 틱 간격을 유지한다.
        next_tick += frame_time;
        // 다음 예약 시각까지 이미 지났다면 현재 시각으로 보정한다. 다음 반복은 시간 대기 없이 진행할 수 있다.
        if (next_tick < tick_start_time) next_tick = tick_start_time;

        // 새 틱이 생겼으니 GameThread들을 깨운다.
        // 사용 가능한 스레드를 확보하고 같은 틱의 처리 정보를 전달한다. 이전 틱의 전체 완료는 기다리지 않는다.
		manager_.lobby_thread_manager_.StartLobbyPhase(tick_time_ms);
        manager_.game_thread_manager_.StartGamePhase(tick_time_ms);
    }

    CloseHandle(high_resolution_timer);
}

void TimerThread::Close()
{
    // 종료 요청만 남긴다. 진행 중인 타이머 대기는 취소하지 않으므로 대기에서 돌아온 뒤 종료 조건을 확인한다.
    is_running_ = false;
}
