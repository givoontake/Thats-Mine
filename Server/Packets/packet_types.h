#pragma once
#include <cstdint>

constexpr std::uint8_t S2C_ERROR = 0;
constexpr std::uint8_t S2C_LOGIN = 1;
constexpr std::uint8_t C2S_LOGIN = 2;
constexpr std::uint8_t S2C_MESSAGE = 3;
constexpr std::uint8_t C2S_MESSAGE = 4;
constexpr std::uint8_t S2C_LOGIN_READY = 5;
constexpr std::uint8_t C2S_DISCONNECT = 8;
constexpr std::uint8_t C2S_ADD_PUBLIC_ROOM = 9;
constexpr std::uint8_t S2C_ADD_PUBLIC_ROOM = 10;
constexpr std::uint8_t C2S_ADD_PRIVATE_ROOM = 11;
constexpr std::uint8_t S2C_ADD_PRIVATE_ROOM = 12;
constexpr std::uint8_t C2S_JOIN_PUBLIC_ROOM = 13;
constexpr std::uint8_t S2C_ADD_PLAYER = 14;
constexpr std::uint8_t C2S_REMOVE_PLAYER = 15;
constexpr std::uint8_t S2C_REMOVE_PLAYER = 16;
constexpr std::uint8_t C2S_READY = 17;
constexpr std::uint8_t S2C_READY = 18;
constexpr std::uint8_t C2S_START = 19;
constexpr std::uint8_t C2S_KICK = 21;
constexpr std::uint8_t S2C_MULTI_START = 39;
constexpr std::uint8_t S2C_UPDATE_HOST = 43;
constexpr std::uint8_t C2S_REQUEST_ROOM_LIST = 47;
constexpr std::uint8_t S2C_ROOM_INFO = 49;
constexpr std::uint8_t C2S_JOIN_PRIVATE_ROOM = 50;
constexpr std::uint8_t C2S_FAST_MATCHING = 51;  
constexpr std::uint8_t C2S_REQUEST_LOBBY_PLAYER_LIST = 58;
constexpr std::uint8_t S2C_LOBBY_PLAYER_INFO = 59;
constexpr std::uint8_t C2S_MOVE = 60;
constexpr std::uint8_t S2C_MOVE = 61;
constexpr std::uint8_t C2S_ACTION = 62;
constexpr std::uint8_t S2C_ACTION = 63;
constexpr std::uint8_t C2S_MOVE_INPUT = 64; // 입력 변화가 있는 이동 상태. C2S_MOVE_PACKET과 같은 구조이며 중계 간격 없이 처리한다.
constexpr std::uint8_t S2C_SPAWN = 65;
constexpr std::uint8_t S2C_INFO = 100;
