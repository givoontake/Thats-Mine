cmake_minimum_required(VERSION 3.21)
project(MapDataExporter LANGUAGES CXX)

set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui")

add_executable(MapDataExporter WIN32
    main.cpp
    ExporterApp.cpp
    EditorProcess.cpp
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_win32.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_dx11.cpp")
target_include_directories(MapDataExporter PRIVATE "${IMGUI_DIR}" "${IMGUI_DIR}/backends")
target_compile_features(MapDataExporter PRIVATE cxx_std_17)
target_compile_definitions(MapDataExporter PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN EXPORTER_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(MapDataExporter PRIVATE d3d11 dxgi)

if(MSVC)
    target_compile_options(MapDataExporter PRIVATE /utf-8 /permissive-)
    set_source_files_properties(main.cpp ExporterApp.cpp EditorProcess.cpp PROPERTIES COMPILE_OPTIONS "/W4")
endif()
