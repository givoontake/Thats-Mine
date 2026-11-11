#pragma once
// 게임 한 판의 진행 시간. 로딩, 시작 대기, 제한 시간, 종료, 시상식, 영역 지우기.

constexpr int LOADING_TIMEOUT_MS = 60000; // 게임 시작 후 전원의 로딩 완료를 기다리는 최대 시간. 지나면 남은 플레이어를 기다리지 않고 시작 대기에 들어간다.
constexpr int GAME_READY_WAIT_MS = 30000; // 시작 대기에 들어간 뒤 실제 게임 시작까지 기다리는 시간
constexpr int GAME_TIME_LIMIT_MS = 7200000; // 게임이 시작된 뒤 보물을 찾지 못해도 게임을 끝내는 시간. 끝나면 보물 위치를 알리고 GAME_END_WAIT_MS 뒤 방으로 돌아간다.
constexpr int GAME_END_WAIT_MS = 10000; // 보물을 발견한 뒤 게임을 끝내고 시상식으로 넘어갈 때까지 기다리는 시간
constexpr int AWARD_SCENE_MS = 5000; // 시상식에서 부문 하나를 보여 주는 시간
constexpr int AWARD_END_MARGIN_MS = 1000; // 시상식이 끝난 뒤 방으로 돌려보낼 때까지 더 기다리는 시간. 마지막 화면 전환이 끝나게 둔다.

// 영역 지우기. 게임 시작부터 AREA_REMOVAL_INTERVAL_MS마다 배제 판 칸 중 보물 칸과 이미 지운 칸을 뺀 칸 하나를 골라 플레이 중인 전원의 배제 판에서 지운다.
// 지울 칸이 없으면 그 차례는 건너뛴다.
constexpr int AREA_REMOVAL_INTERVAL_MS = 180000;
