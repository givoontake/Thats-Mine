#include "ExporterApp.h"
#include "exporter_settings.h"
#include "text_conversion.h"
#include "imgui.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <unordered_set>

// -key="value" 형식 인자. 언리얼은 원래 명령줄을 그대로 읽으므로 따옴표를 = 뒤에 둔다.
static std::wstring QuotedArgument(const wchar_t* key, const std::wstring& value)
{
    return std::wstring(L"-") + key + L"=\"" + value + L"\"";
}

static std::string ToLower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

static std::string KindText(int kinds)
{
    std::string text;
    for (const auto& [bit, name] : { std::pair{ 1, "메시" }, std::pair{ 2, "폴리지" }, std::pair{ 4, "충돌 부품" }, std::pair{ 8, "지형 풀" } })
    {
        if (kinds & bit)
        {
            text += text.empty() ? name : std::string(" + ") + name;
        }
    }
    return text;
}

// 기본 기록 보기에서는 실행한 명령 줄과 추출 명령이 남긴 줄, 오류 줄만 보여 준다. 언리얼 실행 끝에 오류를 다시 모아 찍는 LogInit 줄은 뺀다.
static bool IsExportLine(const std::string& line)
{
    return line.rfind("> ", 0) == 0 || (line.find("LogInit:") == std::string::npos
        && (line.find("LogMapExport:") != std::string::npos || line.find("LogCharacterExport:") != std::string::npos || line.find("LogBoxShapeExport:") != std::string::npos
        || line.find(": Error:") != std::string::npos));
}

// "[시각][프레임]" 머리를 떼어 읽기 쉽게 한다.
static const char* StripTimestamp(const std::string& line)
{
    size_t offset = 0;
    for (int bracket = 0; bracket < 2 && offset < line.size() && line[offset] == '['; ++bracket)
    {
        const size_t end = line.find(']', offset);
        if (end == std::string::npos)
        {
            break;
        }
        offset = end + 1;
    }
    return line.c_str() + offset;
}

ExporterApp::ExporterApp()
    : work_dir_(std::filesystem::temp_directory_path() / "MapDataExporter")
{
    std::filesystem::create_directories(work_dir_);
    strcpy_s(map_path_, DEFAULT_MAP_PATH);
    strcpy_s(output_dir_, ToUtf8(DEFAULT_OUTPUT_DIR.wstring()).c_str());
    status_ = "목록 불러오기로 맵 안의 항목을 읽어 오세요. 에디터가 켜져 있으면 끄고 실행하세요.";
}

bool ExporterApp::StartEditor(const wchar_t* commandlet, const std::wstring& arguments, Task task, bool should_keep_log)
{
    const std::wstring command_line = L"\"" + std::wstring(EDITOR_CMD_PATH) + L"\" \"" + PROJECT_PATH.wstring() + L"\" -run=" + commandlet + L" " + arguments + L" " + EDITOR_OPTIONS;
    if (!should_keep_log)
    {
        log_lines_.clear();
    }
    log_lines_.push_back("> " + ToUtf8(command_line));
    if (!process_.Start(command_line))
    {
        status_ = "에디터를 실행하지 못했습니다.";
        return false;
    }
    task_ = task;
    return true;
}

void ExporterApp::StartList()
{
    const std::wstring arguments = QuotedArgument(L"map", ToWide(map_path_)) + L" -mode=list " + QuotedArgument(L"out", (work_dir_ / "list.txt").wstring())
        + L" -playmin=" + PLAY_MIN + L" -playmax=" + PLAY_MAX;
    if (StartEditor(L"MapExport", arguments, Task::LIST, false))
    {
        status_ = "목록 불러오는 중... (에디터 실행에 30초 가량 걸립니다)";
    }
}

void ExporterApp::StartExport()
{
    // 충돌 모양과 배치를 쓸 항목 이름을 각각 한 줄에 하나씩 적는다.
    const std::filesystem::path collisions_path = work_dir_ / "collisions.txt";
    const std::filesystem::path placements_path = work_dir_ / "placements.txt";
    std::ofstream collisions_file(collisions_path, std::ios::binary);
    std::ofstream placements_file(placements_path, std::ios::binary);
    int collision_count = 0;
    int placement_count = 0;
    for (const ListEntry& entry : entries_)
    {
        if (entry.is_collision_selected)
        {
            collisions_file << entry.name << '\n';
            ++collision_count;
        }
        if (entry.is_placement_selected)
        {
            placements_file << entry.name << '\n';
            ++placement_count;
        }
    }
    collisions_file.close();
    placements_file.close();
    if (collision_count == 0 && placement_count == 0 && !should_export_landscape_ && !should_export_regions_)
    {
        if (should_export_character_)
        {
            StartCharacterExport(false);
            return;
        }
        if (should_export_box_shapes_)
        {
            StartBoxShapeExport(false);
            return;
        }
        status_ = "추출할 지형이나 영역, 충돌 모양, 배치, 캐릭터, 상자 충돌 모양을 고르세요.";
        return;
    }
    std::wstring arguments = QuotedArgument(L"map", ToWide(map_path_)) + L" -mode=export " + QuotedArgument(L"out", ToWide(output_dir_))
        + L" " + QuotedArgument(L"collisions", collisions_path.wstring()) + L" " + QuotedArgument(L"placements", placements_path.wstring())
        + L" -playmin=" + PLAY_MIN + L" -playmax=" + PLAY_MAX;
    arguments += should_export_landscape_ ? L" -landscape" : L"";
    arguments += should_export_regions_ ? L" -regions" : L"";
    arguments += should_rebuild_grass_ ? L" -rebuildgrass" : L"";
    if (StartEditor(L"MapExport", arguments, Task::EXPORT, false))
    {
        status_ = "추출 중... 지형 " + std::string(should_export_landscape_ ? "포함" : "제외") + ", 영역 " + (should_export_regions_ ? "포함" : "제외") + ", 충돌 모양 " + std::to_string(collision_count) + "개, 배치 " + std::to_string(placement_count) + "개"
            + (should_export_character_ ? ", 끝나면 캐릭터" : "") + (should_export_box_shapes_ ? ", 끝나면 상자 충돌 모양" : "");
    }
}

void ExporterApp::StartCharacterExport(bool should_keep_log)
{
    const std::wstring arguments = QuotedArgument(L"out", (std::filesystem::path(ToWide(output_dir_)) / CHARACTER_FILE_NAME).wstring())
        + L" " + QuotedArgument(L"mesh", CHARACTER_MESH_PATH) + L" " + QuotedArgument(L"hitbox", CHARACTER_HITBOX_PATH);
    if (StartEditor(L"CharacterExport", arguments, Task::CHARACTER, should_keep_log))
    {
        status_ = "캐릭터 판정 데이터 추출 중...";
    }
}

void ExporterApp::StartBoxShapeExport(bool should_keep_log)
{
    const std::wstring arguments = QuotedArgument(L"out", (std::filesystem::path(ToWide(output_dir_)) / BOX_SHAPE_FILE_NAME).wstring());
    if (StartEditor(L"BoxShapeExport", arguments, Task::BOX_SHAPES, should_keep_log))
    {
        status_ = "상자 충돌 모양 추출 중...";
    }
}

void ExporterApp::FinishTask(DWORD exit_code)
{
    const Task task = task_;
    task_ = Task::NONE;
    if (task == Task::LIST)
    {
        if (exit_code == 0)
        {
            LoadList();
            status_ = "목록을 불러왔습니다. 지형 조각 " + std::to_string(landscape_count_) + "개, 항목 " + std::to_string(entries_.size()) + "개";
        }
        else
        {
            status_ = "목록 불러오기 실패 (종료 코드 " + std::to_string(exit_code) + "). 아래 기록을 확인하세요.";
        }
        return;
    }
    RefreshFileFlags();
    if (task == Task::EXPORT && exit_code == 0 && should_export_character_)
    {
        StartCharacterExport(true);
        return;
    }
    if ((task == Task::EXPORT || task == Task::CHARACTER) && exit_code == 0 && should_export_box_shapes_)
    {
        StartBoxShapeExport(true);
        return;
    }
    status_ = exit_code == 0 ? "추출을 마쳤습니다." : "추출 실패 (종료 코드 " + std::to_string(exit_code) + "). 아래 기록을 확인하세요.";
}

// 목록 파일: 첫 줄 "LANDSCAPE<TAB>지형 조각 수",
// 항목마다 "ENTRY<TAB>종류 비트<TAB>이름<TAB>판정 방식(-1 충돌 모양 없음, 0 단순 도형, 1 삼각형)<TAB>충돌 배치 수<TAB>충돌 없는 물체 수(-1이면 지형 풀)<TAB>충돌 모양 경로"
void ExporterApp::LoadList()
{
    // 다시 불러올 때는 고른 상태를 이름으로 이어 받고, 처음에는 파일이 있는 것을 고른다.
    const bool is_first = entries_.empty();
    std::unordered_set<std::string> collision_selected;
    std::unordered_set<std::string> placement_selected;
    for (const ListEntry& entry : entries_)
    {
        if (entry.is_collision_selected)
        {
            collision_selected.insert(entry.name);
        }
        if (entry.is_placement_selected)
        {
            placement_selected.insert(entry.name);
        }
    }
    entries_.clear();

    std::ifstream file(work_dir_ / "list.txt", std::ios::binary);
    std::string line;
    while (std::getline(file, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        std::istringstream row(line);
        std::string fields[7];
        int count = 0;
        while (count < 7 && std::getline(row, fields[count], '\t'))
        {
            ++count;
        }
        if (count == 2 && fields[0] == "LANDSCAPE")
        {
            landscape_count_ = std::stoi(fields[1]);
        }
        else if (count >= 6 && fields[0] == "ENTRY")
        {
            const int trace = std::stoi(fields[3]);
            const int decor_count = std::stoi(fields[5]);
            ListEntry entry;
            entry.name = fields[2];
            entry.kind = KindText(std::stoi(fields[1]));
            entry.collision = trace < 0 ? "" : trace == 1 ? "삼각형" : "단순 도형";
            entry.collision_count = std::stoi(fields[4]);
            entry.decor = decor_count < 0 ? "추출 때 계산" : std::to_string(decor_count);
            entry.path = fields[6];
            entries_.push_back(entry);
        }
    }
    RefreshFileFlags();
    for (ListEntry& entry : entries_)
    {
        entry.is_collision_selected = !entry.collision.empty() && (is_first ? entry.has_collision_file : collision_selected.count(entry.name) > 0);
        entry.is_placement_selected = is_first ? entry.has_placement_file : placement_selected.count(entry.name) > 0;
    }
}

void ExporterApp::RefreshFileFlags()
{
    const std::filesystem::path meshes_dir = std::filesystem::path(ToWide(output_dir_)) / L"Map" / L"Meshes";
    for (ListEntry& entry : entries_)
    {
        const std::filesystem::path entry_dir = meshes_dir / ToWide(entry.name);
        entry.has_collision_file = std::filesystem::exists(entry_dir / L"collision.bin");
        entry.has_placement_file = std::filesystem::exists(entry_dir / L"placements.bin");
    }
}

bool ExporterApp::IsVisible(const ListEntry& entry) const
{
    return filter_[0] == '\0' || ToLower(entry.name).find(ToLower(filter_)) != std::string::npos;
}

void ExporterApp::Draw()
{
    for (std::string& line : process_.TakeLines())
    {
        log_lines_.push_back(std::move(line));
        should_scroll_log_ = true;
    }
    if (task_ != Task::NONE && !process_.IsRunning())
    {
        FinishTask(process_.Wait());
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::Begin("MapDataExporter", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);

    ImGui::BeginDisabled(task_ != Task::NONE);
    ImGui::InputText("맵", map_path_, sizeof(map_path_));
    ImGui::InputText("출력 폴더", output_dir_, sizeof(output_dir_));
    if (ImGui::Button("목록 불러오기"))
    {
        StartList();
    }
    ImGui::SameLine();
    if (ImGui::Button("추출"))
    {
        StartExport();
    }
    ImGui::SameLine();
    ImGui::Checkbox("지형(랜드스케이프)", &should_export_landscape_);
    ImGui::SameLine();
    ImGui::Checkbox("영역(스플라인)", &should_export_regions_);
    ImGui::SameLine();
    ImGui::Checkbox("낡은 풀 맵 다시 만들기(맵 저장)", &should_rebuild_grass_);
    ImGui::SameLine();
    ImGui::Checkbox("캐릭터 판정 데이터", &should_export_character_);
    ImGui::SameLine();
    ImGui::Checkbox("상자 충돌 모양", &should_export_box_shapes_);
    ImGui::EndDisabled();
    ImGui::TextUnformatted(status_.c_str());
    ImGui::Separator();

    DrawTable();
    DrawLog();
    ImGui::End();
}

void ExporterApp::DrawTable()
{
    ImGui::SetNextItemWidth(260.0f);
    ImGui::InputTextWithHint("##filter", "이름 거르기", filter_, sizeof(filter_));
    ImGui::SameLine();
    if (ImGui::Button("보이는 항목 모두 선택"))
    {
        for (ListEntry& entry : entries_)
        {
            if (IsVisible(entry))
            {
                entry.is_collision_selected = !entry.collision.empty();
                entry.is_placement_selected = true;
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("보이는 항목 해제"))
    {
        for (ListEntry& entry : entries_)
        {
            if (IsVisible(entry))
            {
                entry.is_collision_selected = false;
                entry.is_placement_selected = false;
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("파일 있는 것만 선택"))
    {
        for (ListEntry& entry : entries_)
        {
            entry.is_collision_selected = entry.has_collision_file;
            entry.is_placement_selected = entry.has_placement_file;
        }
    }
    ImGui::SameLine();
    ImGui::Text("항목 %d, 모양 선택 %d, 배치 선택 %d", static_cast<int>(entries_.size()),
        static_cast<int>(std::count_if(entries_.begin(), entries_.end(), [](const ListEntry& entry) { return entry.is_collision_selected; })),
        static_cast<int>(std::count_if(entries_.begin(), entries_.end(), [](const ListEntry& entry) { return entry.is_placement_selected; })));

    std::vector<int> visible;
    for (int index = 0; index < static_cast<int>(entries_.size()); ++index)
    {
        if (IsVisible(entries_[index]))
        {
            visible.push_back(index);
        }
    }
    constexpr float LOG_HEIGHT = 230.0f;
    const ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;
    if (!ImGui::BeginTable("entries", 9, flags, ImVec2(0.0f, ImGui::GetContentRegionAvail().y - LOG_HEIGHT)))
    {
        return;
    }
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("모양", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("배치", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn("이름", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("종류", ImGuiTableColumnFlags_WidthFixed, 120.0f);
    ImGui::TableSetupColumn("판정", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("충돌 배치", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("충돌 없는 물체", ImGuiTableColumnFlags_WidthFixed, 110.0f);
    ImGui::TableSetupColumn("파일", ImGuiTableColumnFlags_WidthFixed, 90.0f);
    ImGui::TableSetupColumn("충돌 모양 경로", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(visible.size()));
    while (clipper.Step())
    {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
        {
            ListEntry& entry = entries_[visible[row]];
            ImGui::TableNextRow();
            ImGui::PushID(visible[row]);
            ImGui::TableNextColumn();
            ImGui::BeginDisabled(entry.collision.empty());
            ImGui::Checkbox("##collision", &entry.is_collision_selected);
            ImGui::EndDisabled();
            ImGui::TableNextColumn();
            ImGui::Checkbox("##placement", &entry.is_placement_selected);
            ImGui::PopID();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(entry.name.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(entry.kind.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(entry.collision.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%d", entry.collision_count);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(entry.decor.c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(entry.has_collision_file && entry.has_placement_file ? "모양+배치" : entry.has_collision_file ? "모양" : entry.has_placement_file ? "배치" : "");
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(entry.path.c_str());
        }
    }
    ImGui::EndTable();
}

void ExporterApp::DrawLog()
{
    ImGui::Checkbox("전체 기록 보기", &should_show_all_log_);
    ImGui::SameLine();
    if (ImGui::Button("기록 지우기"))
    {
        log_lines_.clear();
    }
    ImGui::BeginChild("log", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
    for (size_t index = 0; index < log_lines_.size(); ++index)
    {
        if (should_show_all_log_ || IsExportLine(log_lines_[index]))
        {
            ImGui::TextUnformatted(StripTimestamp(log_lines_[index]));
        }
    }
    if (should_scroll_log_)
    {
        ImGui::SetScrollHereY(1.0f);
        should_scroll_log_ = false;
    }
    ImGui::EndChild();
}
