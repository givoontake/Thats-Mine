cmake_minimum_required(VERSION 3.21)
project(GameServer LANGUAGES CXX)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "GameServer requires 64-bit Windows.")
endif()

find_package(TBB CONFIG REQUIRED)
find_package(Botan CONFIG REQUIRED)
set(MYSQL_CONNECTOR_ROOT "" CACHE PATH "MySQL Connector/C++ installation directory")
find_path(MYSQL_CONNECTOR_INCLUDE_DIR jdbc/mysql_driver.h HINTS "${MYSQL_CONNECTOR_ROOT}/include" REQUIRED)
find_library(MYSQL_CONNECTOR_LIBRARY mysqlcppconn HINTS "${MYSQL_CONNECTOR_ROOT}/lib64/vs14" REQUIRED)
find_file(MYSQL_CONNECTOR_DLL NAMES mysqlcppconn-10-vs14.dll mysqlcppconn-9-vs14.dll HINTS "${MYSQL_CONNECTOR_ROOT}/lib64" REQUIRED)

add_library(MySQLConnector SHARED IMPORTED)
set_target_properties(MySQLConnector PROPERTIES
    IMPORTED_IMPLIB "${MYSQL_CONNECTOR_LIBRARY}"
    IMPORTED_LOCATION "${MYSQL_CONNECTOR_DLL}"
    INTERFACE_INCLUDE_DIRECTORIES "${MYSQL_CONNECTOR_INCLUDE_DIR}"
)

add_executable(GameServer
    Core/main.cpp
    Core/GameServer.cpp
    Game/Player.cpp
    Game/Boxes/BoxCandidateData.cpp
    Game/Hitbox/CharacterPose.cpp
    Game/Hitbox/HitboxData.cpp
    Game/Hitbox/hitbox_raycast.cpp
    Game/Regions/MapRegionData.cpp
    Game/Terrain/TerrainData.cpp
    Lobby/LoginManager.cpp
    Lobby/Matchmaker.cpp
    Managers/ActivePlayerManager.cpp
    Network/Session.cpp
    Network/SendBuffer.cpp
    Network/SessionManager.cpp
    Rooms/GameRoom.cpp
    Rooms/GameRoomPackets.cpp
    Threads/ServerThread.cpp
    Threads/Database/DBThread.cpp
    Threads/Database/LoginDBThread.cpp
    Database/DBResultHandler.cpp
    Threads/Game/GameThread.cpp
    Threads/Game/GamePhaseContext.cpp
    Threads/IO/IOThread.cpp
    Threads/Lobby/LobbyThread.cpp
    Threads/Lobby/LobbyPhaseContext.cpp
    Threads/Timer/TimerThread.cpp
    Threads/Managers/ServerThreadManager.cpp
    Threads/Managers/DBThreadManager.cpp
    Threads/Managers/GameThreadManager.cpp
    Threads/Managers/IOThreadManager.cpp
    Threads/Managers/LobbyThreadManager.cpp
    Threads/Managers/TimerThreadManager.cpp
    Common/ActiveList.h
    Common/char_buffer.h
    Common/ConcurrentTaskQueue.h
    Common/enum_class.h
    Common/error_code.h
    Common/IndexRegistry.h
    Common/info_code.h
    Common/settings.h
    Common/platform_coordinates.h
    Common/platform_layout.h
    Common/spawn_points.h
    Common/types.h
    Core/GameServer.h
    Database/db_login_result.h
    Database/db_login_task.h
    Database/DBOverlapped.h
    Database/DBResultHandler.h
    Game/Player.h
    Game/Boxes/box_types.h
    Game/Boxes/BoxCandidateData.h
    Game/Hitbox/CharacterPose.h
    Game/Hitbox/HitboxData.h
    Game/Hitbox/hitbox_raycast.h
    Game/Hitbox/hitbox_types.h
    Game/Regions/MapRegionData.h
    Game/Regions/region_types.h
    Game/Terrain/TerrainData.h
    Game/Terrain/terrain_types.h
    Lobby/lobby_tasks.h
    Lobby/LoginManager.h
    Lobby/Matchmaker.h
    Managers/ActivePlayerManager.h
    Network/ExOverlapped.h
    Network/SendBuffer.h
    Network/session_tasks.h
    Network/Session.h
    Network/SessionManager.h
    Packets/common_packets.h
    Packets/database_packets.h
    Packets/game_packets.h
    Packets/lobby_packets.h
    Packets/packet_types.h
    Packets/room_packets.h
    Rooms/GameRoom.h
    Rooms/room_lifecycle_tasks.h
    Threads/Database/DBThread.h
    Threads/Database/LoginDBThread.h
    Threads/Game/game_state.h
    Threads/Game/GamePhaseContext.h
    Threads/Game/GameThread.h
    Threads/IO/IOThread.h
    Threads/Lobby/lobby_state.h
    Threads/Lobby/LobbyPhaseContext.h
    Threads/Lobby/LobbyThread.h
    Threads/Managers/DBThreadManager.h
    Threads/Managers/GameThreadManager.h
    Threads/Managers/IOThreadManager.h
    Threads/Managers/LobbyThreadManager.h
    Threads/Managers/ServerThreadManager.h
    Threads/Managers/TimerThreadManager.h
    Threads/ServerThread.h
    Threads/Timer/TimerThread.h
)
get_target_property(GAME_SERVER_FILES GameServer SOURCES)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${GAME_SERVER_FILES})
target_compile_features(GameServer PRIVATE cxx_std_20)
target_compile_definitions(GameServer PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_include_directories(GameServer PRIVATE
    . Common Core Database Game Game/Boxes Game/Hitbox Game/Regions Game/Terrain Lobby Managers Network Packets Rooms Threads
    Threads/Database Threads/Game Threads/IO Threads/Lobby Threads/Timer Threads/Managers
)
target_link_libraries(GameServer PRIVATE TBB::tbb Botan::Botan MySQLConnector ws2_32 mswsock bcrypt)
if(MSVC)
    target_compile_options(GameServer PRIVATE /W4 /utf-8 /permissive-)
endif()

add_custom_command(TARGET GameServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:GameServer> $<TARGET_FILE_DIR:GameServer>
    COMMAND_EXPAND_LISTS
)
get_filename_component(MYSQL_CONNECTOR_RUNTIME_DIR "${MYSQL_CONNECTOR_DLL}" DIRECTORY)
file(GLOB MYSQL_CONNECTOR_TLS_DLLS "${MYSQL_CONNECTOR_RUNTIME_DIR}/libssl*.dll" "${MYSQL_CONNECTOR_RUNTIME_DIR}/libcrypto*.dll")
if(MYSQL_CONNECTOR_TLS_DLLS)
    add_custom_command(TARGET GameServer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MYSQL_CONNECTOR_TLS_DLLS} $<TARGET_FILE_DIR:GameServer>
        COMMAND_EXPAND_LISTS
    )
endif()
set_property(TARGET GameServer PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
