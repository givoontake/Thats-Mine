#include <cstring>
#include <iostream>
#include <memory>
#include "GameServer.h"
#include "HintObjectTree.h"
#include "MapData.h"

HANDLE console_close_event = nullptr;

BOOL WINAPI HandleConsoleClose(DWORD signal)
{
    switch (signal) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        if (console_close_event) SetEvent(console_close_event);
        return TRUE;

    default:
        return FALSE;
    }
}

// 서버를 띄우지 않고 맵 데이터로 힌트 물체 트리를 만들어 클라이언트 돋보기용 파일로 쓴다.
static int ExportHintObjects(const char* file_path)
{
    MapData map;
    HintObjectTree tree;
    if (!map.Load("Data/Map") || !tree.Build(map) || !tree.Export(file_path)) {
        std::cerr << "Hint object export fail.. " << std::endl;
        return 1;
    }
    std::cout << "Hint objects exported: " << file_path << std::endl;
    return 0;
}

int main(int argc, char* argv[])
{
    SetConsoleOutputCP(65001);
    if (argc == 3 && std::strcmp(argv[1], "--export-hint-objects") == 0) return ExportHintObjects(argv[2]);

    console_close_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!console_close_event) return 1;
    if (!SetConsoleCtrlHandler(HandleConsoleClose, TRUE)) {
        CloseHandle(console_close_event);
        console_close_event = nullptr;
        return 1;
    }

    auto game_server = std::make_unique<GameServer>();
    game_server->StartServer();

    game_server->StartThreads();

    while (game_server->IsRunning()) {
        const DWORD wait_result = WaitForSingleObject(console_close_event, 100);
        if (wait_result == WAIT_OBJECT_0 || wait_result == WAIT_FAILED) break;
    }

    game_server->CloseThreads();
    game_server->JoinThreads();

    SetConsoleCtrlHandler(HandleConsoleClose, FALSE);
    CloseHandle(console_close_event);
    console_close_event = nullptr;

    return 0;
}
