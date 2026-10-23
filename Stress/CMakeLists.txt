cmake_minimum_required(VERSION 3.21)
project(Stress LANGUAGES CXX)

# 이동 부하 시험 봇. 서버 패킷 헤더와 설정을 그대로 쓴다.
add_executable(Stress
    main.cpp
    LatencyRecorder.cpp
    Session.cpp
    TestManager.cpp
    ex_overlapped.h
    LatencyRecorder.h
    Session.h
    stress_settings.h
    TestManager.h
)
set_target_properties(Stress PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
target_include_directories(Stress PRIVATE ../Server/Common ../Server/Packets)
target_compile_definitions(Stress PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(Stress PRIVATE /utf-8 /W4)
target_link_libraries(Stress PRIVATE ws2_32 mswsock)
