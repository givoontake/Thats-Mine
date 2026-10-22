#pragma once

#include <filesystem>

// EXPORTER_ROOT는 CMake가 이 프로그램 폴더(MapDataExporter) 경로로 넣는다.
inline const std::filesystem::path REPOSITORY_ROOT = std::filesystem::path(EXPORTER_ROOT).make_preferred().parent_path();
inline const std::filesystem::path PROJECT_PATH = REPOSITORY_ROOT / "Client" / "Thats_Mine.uproject";
inline const std::filesystem::path DEFAULT_OUTPUT_DIR = REPOSITORY_ROOT / "LocalWork" / "MapExport";

constexpr wchar_t EDITOR_CMD_PATH[] = L"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe";
constexpr char DEFAULT_MAP_PATH[] = "/Game/Thats_Mine/Levels/Demo_Landmass";

// 캐릭터 판정 데이터를 뽑을 메시와 히트박스 물리 에셋, 출력 폴더 안의 파일 이름
constexpr wchar_t CHARACTER_MESH_PATH[] = L"/Game/Thats_Mine/Actors/Characters/Barbarian1";
constexpr wchar_t CHARACTER_HITBOX_PATH[] = L"/Game/Thats_Mine/Actors/Characters/PA_BarbarianHitbox";
constexpr wchar_t CHARACTER_FILE_NAME[] = L"hitbox_data.bin";
// 상자 충돌 모양을 쓸 출력 폴더 안의 파일 이름
constexpr wchar_t BOX_SHAPE_FILE_NAME[] = L"box_shapes.txt";

// 충돌 없는 물체를 넣을 플레이 영역(맵 경계 벽 안쪽 면, x,y cm)
constexpr wchar_t PLAY_MIN[] = L"-100750,-100700";
constexpr wchar_t PLAY_MAX[] = L"100650,100700";

// 화면 없는 에디터 실행 옵션. 풀 맵을 다시 만들 때 재질 텍스처가 다 올라오도록 텍스처 스트리밍을 끈다.
constexpr wchar_t EDITOR_OPTIONS[] = L"-AllowCommandletRendering -RenderOffscreen -unattended -nop4 -nosplash -NoSound -dpcvars=r.TextureStreaming=0";
