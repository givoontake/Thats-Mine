#pragma once
#include <cstddef>

// 로그인·로비·방·빠른 매칭.

constexpr int MAX_ROOM_PLAYER_COUNT = 20;
constexpr int MIN_START_PLAYER_COUNT = 1; // 게임을 시작할 수 있는 최소 인원. 당분간 혼자서도 시작할 수 있게 1로 둔다(원래 2).
constexpr int MAX_ROOM_NAME_SIZE = 48; // 16자 * UTF-8 1문자 크기(3)
constexpr int MAX_ROOM_PASSWORD_SIZE = 48;
// 방 이름·비밀번호의 최소 길이(바이트). 이보다 짧으면 방을 만들지 않는다.
constexpr std::size_t MIN_ROOM_NAME_LENGTH = 4;
constexpr std::size_t MIN_ROOM_PASSWORD_LENGTH = 4;
constexpr int ROOM_LIST_REFRESH_INTERVAL_MS = 1000; // 로비가 방 목록 응답을 새로 만드는 간격. 방 목록 요청에는 마지막으로 만든 목록을 보낸다.

constexpr int MAX_PLAYER_NAME_SIZE = 48;
constexpr int MAX_PLAYER_ID_SIZE = 48;
constexpr int MAX_PLAYER_PASSWORD_SIZE = 48;

// 빠른 매칭이 이 시간 안에 들어갈 방을 찾지 못하면 매칭을 끝내고 로비로 돌려보낸다.
constexpr long long MATCH_TIMEOUT_MS = 5000;
