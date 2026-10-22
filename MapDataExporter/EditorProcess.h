#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <windows.h>

// 화면 없는 에디터를 자식 프로세스로 띄우고, 표준 출력(UTF-8)을 줄 단위로 모은다.
class EditorProcess
{
    HANDLE process_ = nullptr;
    HANDLE output_read_ = nullptr;
    std::thread reader_;
    std::mutex mutex_;
    std::vector<std::string> pending_lines_;
    std::atomic<bool> is_running_ = false;
    DWORD exit_code_ = 0;

    void ReadOutput();

public:
    ~EditorProcess();

    bool Start(const std::wstring& command_line);
    bool IsRunning() const { return is_running_; }
    // 끝난 프로세스를 정리하고 종료 코드를 돌려준다. 아직 실행 중이면 끝날 때까지 기다린다.
    DWORD Wait();
    // 지난번 이후 새로 나온 줄을 넘겨준다.
    std::vector<std::string> TakeLines();
};
