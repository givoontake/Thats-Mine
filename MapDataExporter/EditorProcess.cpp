#include "EditorProcess.h"

#include <utility>

EditorProcess::~EditorProcess()
{
    // 맵 저장 중에 끊으면 맵이 깨질 수 있으므로 강제로 끝내지 않고 기다린다.
    Wait();
}

bool EditorProcess::Start(const std::wstring& command_line)
{
    if (is_running_ || process_)
    {
        return false;
    }
    SECURITY_ATTRIBUTES attributes = { sizeof(attributes), nullptr, TRUE };
    HANDLE output_write = nullptr;
    if (!CreatePipe(&output_read_, &output_write, &attributes, 0))
    {
        return false;
    }
    SetHandleInformation(output_read_, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW startup = { sizeof(startup) };
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = output_write;
    startup.hStdError = output_write;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION info = {};
    std::wstring mutable_command_line = command_line;
    const BOOL is_created = CreateProcessW(nullptr, mutable_command_line.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &info);
    CloseHandle(output_write);
    if (!is_created)
    {
        CloseHandle(output_read_);
        output_read_ = nullptr;
        return false;
    }
    CloseHandle(info.hThread);
    process_ = info.hProcess;
    is_running_ = true;
    reader_ = std::thread(&EditorProcess::ReadOutput, this);
    return true;
}

// 출력 관이 닫힐 때(프로세스 끝)까지 읽어 줄로 나눈다.
void EditorProcess::ReadOutput()
{
    std::string partial;
    char buffer[4096];
    DWORD read = 0;
    while (ReadFile(output_read_, buffer, sizeof(buffer), &read, nullptr) && read > 0)
    {
        partial.append(buffer, read);
        size_t line_end = 0;
        std::lock_guard<std::mutex> lock(mutex_);
        while ((line_end = partial.find('\n')) != std::string::npos)
        {
            std::string line = partial.substr(0, line_end);
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }
            pending_lines_.push_back(std::move(line));
            partial.erase(0, line_end + 1);
        }
    }
    WaitForSingleObject(process_, INFINITE);
    GetExitCodeProcess(process_, &exit_code_);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!partial.empty())
        {
            pending_lines_.push_back(partial);
        }
    }
    is_running_ = false;
}

DWORD EditorProcess::Wait()
{
    if (reader_.joinable())
    {
        reader_.join();
    }
    if (process_)
    {
        CloseHandle(process_);
        process_ = nullptr;
    }
    if (output_read_)
    {
        CloseHandle(output_read_);
        output_read_ = nullptr;
    }
    return exit_code_;
}

std::vector<std::string> EditorProcess::TakeLines()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return std::exchange(pending_lines_, {});
}
