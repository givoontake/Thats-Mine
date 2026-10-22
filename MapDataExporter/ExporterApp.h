#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include "EditorProcess.h"

// 목록 항목 하나. 출력 폴더 Map/Meshes/이름 하나에 해당한다.
struct ListEntry
{
    std::string name;
    std::string kind; // 메시·폴리지·충돌 부품·지형 풀
    std::string collision; // 충돌 모양 판정 방식. 충돌 모양이 없으면 빈 문자열
    int collision_count = 0; // 충돌 배치 수
    std::string decor; // 충돌 없는 물체 수
    std::string path; // 충돌 모양 원본 경로
    bool has_collision_file = false;
    bool has_placement_file = false;
    bool is_collision_selected = false;
    bool is_placement_selected = false;
};

// 추출 프로그램 화면. 에디터를 화면 없이 띄워 목록을 받고, 고른 지형과 항목별 충돌 모양·배치, 캐릭터 판정 데이터, 상자 충돌 모양을 추출시킨다.
class ExporterApp
{
    enum class Task { NONE, LIST, EXPORT, CHARACTER, BOX_SHAPES };

    EditorProcess process_;
    Task task_ = Task::NONE;
    std::filesystem::path work_dir_; // 목록·선택 임시 파일
    char map_path_[256] = {};
    char output_dir_[512] = {};
    char filter_[128] = {};
    bool should_export_landscape_ = true;
    bool should_export_regions_ = true;
    bool should_rebuild_grass_ = true;
    bool should_export_character_ = false;
    bool should_export_box_shapes_ = false;
    bool should_show_all_log_ = false;
    bool should_scroll_log_ = false;
    int landscape_count_ = 0;
    std::vector<ListEntry> entries_;
    std::vector<std::string> log_lines_;
    std::string status_;

    // 기록은 맵 추출에 이어 캐릭터·상자 충돌 모양을 추출할 때만 이어 쓴다.
    bool StartEditor(const wchar_t* commandlet, const std::wstring& arguments, Task task, bool should_keep_log);
    void StartList();
    void StartExport();
    void StartCharacterExport(bool should_keep_log);
    void StartBoxShapeExport(bool should_keep_log);
    void FinishTask(DWORD exit_code);
    void LoadList();
    void RefreshFileFlags();
    bool IsVisible(const ListEntry& entry) const;
    void DrawTable();
    void DrawLog();

public:
    ExporterApp();
    void Draw();
};
